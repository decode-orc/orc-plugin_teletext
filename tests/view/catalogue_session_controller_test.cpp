/*
 * File:        catalogue_session_controller_test.cpp
 * Module:      orc-plugin_teletext view tests
 * Purpose:     The catalogue browser asks the stage for its catalogue through
 *              the tool session, and shows what comes back
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "catalogue_session_controller.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QLabel>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "catalogue/catalogue_wire.h"
#include "cataloguedialog.h"

namespace {

using orc::teletext_wire::ViewSelection;

QApplication& ensureApplication() {
  if (auto* existing =
          qobject_cast<QApplication*>(QCoreApplication::instance())) {
    return *existing;
  }
  static int argc = 3;
  static char app_name[] = "orc-view-teletext-session-test";
  static char platform_opt[] = "-platform";
  static char platform_val[] = "offscreen";
  static char* argv[] = {app_name, platform_opt, platform_val, nullptr};
  static QApplication* app = [] {
    auto* created = new QApplication(argc, argv);
    created->setQuitOnLastWindowClosed(false);
    return created;
  }();
  return *app;
}

// Holds every request until the test answers it, the way the host's session
// does until the render worker has run it.
class FakeSession : public orc::view::IStageToolSession {
 public:
  struct Request {
    std::string verb;
    ViewSelection selection;
    orc::view::ToolReplyHandler on_reply;
  };

  std::string contract_id() const override {
    return orc::teletext_wire::kCatalogueViewContractId;
  }
  std::string title() const override { return "Teletext Pages"; }
  bool is_open() const override { return true; }
  void request(const std::string& verb, std::vector<uint8_t> payload,
               orc::view::ToolReplyHandler on_reply) override {
    Request r;
    r.verb = verb;
    std::string error;
    EXPECT_TRUE(
        orc::teletext_wire::decode_view_selection(payload, r.selection, &error))
        << error;
    r.on_reply = std::move(on_reply);
    requests.push_back(std::move(r));
  }
  void set_results_changed_handler(std::function<void()> handler) override {
    results_changed = std::move(handler);
  }

  void answer(size_t index, const orc::CatalogueDataset& dataset) {
    orc::view::ToolReply reply;
    reply.ok = true;
    reply.data = orc::teletext_wire::encode_catalogue(dataset);
    requests.at(index).on_reply(std::move(reply));
  }
  void fail(size_t index, const std::string& error) {
    orc::view::ToolReply reply;
    reply.error = error;
    requests.at(index).on_reply(std::move(reply));
  }

  std::vector<Request> requests;
  std::function<void()> results_changed;
};

// Two items, offered under two view options and one toggle.
orc::CatalogueDataset makeCatalogue(const std::string& option, bool toggle_on) {
  orc::CatalogueDataset d;
  d.schema.columns = {{"record", "Record", false}};
  d.schema.item_noun = "Record";
  d.schema.view_label = "Receiver";
  d.schema.view_options = {{"coarse", "Coarse", ""}, {"fine", "Fine", ""}};
  d.schema.view_option = option;
  d.schema.toggles = {{"repair", "Repair", "", toggle_on}};
  for (const char* id : {"A00", "A01"}) {
    orc::CatalogueItem item;
    item.id = id;
    item.values = {id};
    d.items.push_back(item);
    orc::CataloguePayload payload;
    payload.kind = orc::CataloguePayload::Kind::kText;
    payload.document.text = std::string("record ") + id + " under " + option;
    d.payloads.push_back(payload);
  }
  d.summary.headline = "2 records";
  return d;
}

class CatalogueSessionControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ensureApplication();
    session_ = std::make_shared<FakeSession>();
    dialog_ = new CatalogueDialog();
    controller_ = new CatalogueSessionController(dialog_, session_);
  }
  void TearDown() override { delete dialog_; }

  std::shared_ptr<FakeSession> session_;
  CatalogueDialog* dialog_ = nullptr;
  CatalogueSessionController* controller_ = nullptr;
};

TEST_F(CatalogueSessionControllerTest, AsksForTheCatalogueOnOpening) {
  ASSERT_EQ(session_->requests.size(), 1u);
  EXPECT_EQ(session_->requests[0].verb, orc::teletext_wire::kCatalogueVerb);
  EXPECT_TRUE(session_->requests[0].selection.view_option.empty());
  EXPECT_TRUE(session_->requests[0].selection.active_toggles.empty());
  bool waiting = false;
  for (const QLabel* label : dialog_->findChildren<QLabel*>()) {
    waiting = waiting || label->text().startsWith("Decoding");
  }
  EXPECT_TRUE(waiting) << "the dialog says it is waiting";
}

TEST_F(CatalogueSessionControllerTest, ShowsTheCatalogueThatArrives) {
  session_->answer(0, makeCatalogue("coarse", false));
  EXPECT_EQ(dialog_->listedItems().size(), 2u);
  EXPECT_EQ(dialog_->currentViewOption(), "coarse");
}

TEST_F(CatalogueSessionControllerTest, ShowsTheStagesReasonForNothing) {
  session_->fail(0, orc::teletext_wire::kNotTriggeredMessage);
  EXPECT_TRUE(dialog_->listedItems().empty());
  EXPECT_NE(dialog_->headlineText().indexOf("Trigger Stage"), -1)
      << dialog_->headlineText().toStdString();
}

TEST_F(CatalogueSessionControllerTest, SaysSoWhenTheBytesCannotBeRead) {
  orc::view::ToolReply reply;
  reply.ok = true;
  reply.data = {1, 2, 3};
  session_->requests[0].on_reply(std::move(reply));
  EXPECT_TRUE(dialog_->listedItems().empty());
  EXPECT_NE(dialog_->headlineText().indexOf("could not be read"), -1)
      << dialog_->headlineText().toStdString();
}

// Picking another receiver asks the stage for the same catalogue drawn that
// way, carrying the toggles that are on with it.
TEST_F(CatalogueSessionControllerTest, AsksAgainUnderTheReadersSelection) {
  session_->answer(0, makeCatalogue("coarse", true));
  dialog_->selectViewOption("fine");
  ASSERT_EQ(session_->requests.size(), 2u);
  EXPECT_EQ(session_->requests[1].selection.view_option, "fine");
  EXPECT_EQ(session_->requests[1].selection.active_toggles,
            std::vector<std::string>{"repair"});

  // What was on show stays until the new catalogue arrives.
  EXPECT_EQ(dialog_->listedItems().size(), 2u);
  session_->answer(1, makeCatalogue("fine", true));
  EXPECT_EQ(dialog_->currentViewOption(), "fine");
  EXPECT_NE(dialog_->currentText().indexOf("under fine"), -1);
}

TEST_F(CatalogueSessionControllerTest, ATogglesChangeIsAskedFor) {
  session_->answer(0, makeCatalogue("coarse", false));
  dialog_->setToggleActive("repair", true);
  ASSERT_EQ(session_->requests.size(), 2u);
  EXPECT_EQ(session_->requests[1].selection.view_option, "coarse");
  EXPECT_EQ(session_->requests[1].selection.active_toggles,
            std::vector<std::string>{"repair"});
}

// A trigger replaces the stage's results: the browser asks again, keeping the
// reader's way of looking at them.
TEST_F(CatalogueSessionControllerTest, AsksAgainWhenTheStageIsTriggered) {
  session_->answer(0, makeCatalogue("coarse", false));
  dialog_->selectViewOption("fine");
  session_->answer(1, makeCatalogue("fine", false));

  ASSERT_TRUE(session_->results_changed);
  session_->results_changed();
  ASSERT_EQ(session_->requests.size(), 3u);
  EXPECT_EQ(session_->requests[2].selection.view_option, "fine");
}

TEST_F(CatalogueSessionControllerTest, NothingIsDeliveredAfterTheDialogGoes) {
  delete dialog_;
  dialog_ = nullptr;
  // The host closes the session first; a reply that slipped through anyway
  // must find nothing to touch.
  session_->answer(0, makeCatalogue("coarse", false));
  EXPECT_FALSE(session_->results_changed);
}

}  // namespace
