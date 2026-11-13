/*
 * File:        catalogue_session_controller.cpp
 * Module:      orc-view_teletext
 * Purpose:     Feeds a CatalogueDialog from the stage through a tool session
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "catalogue_session_controller.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cataloguedialog.h"

CatalogueSessionController::CatalogueSessionController(
    CatalogueDialog* dialog,
    std::shared_ptr<orc::view::IStageToolSession> session)
    : QObject(dialog), dialog_(dialog), session_(std::move(session)) {
  connect(dialog, &CatalogueDialog::viewSelectionChanged, this,
          &CatalogueSessionController::onViewSelectionChanged);
  connect(dialog, &CatalogueDialog::keysPressed, this,
          &CatalogueSessionController::onKeysPressed);
  connect(dialog, &CatalogueDialog::exportDataRequested, this,
          &CatalogueSessionController::onExportDataRequested);

  QPointer<CatalogueSessionController> self(this);
  session_->set_results_changed_handler([self]() {
    if (self) {
      self->request(self->currentSelection(), true);
    }
  });

  request(orc::teletext_wire::ViewSelection{}, true);
}

CatalogueSessionController::~CatalogueSessionController() {
  if (session_) {
    session_->set_results_changed_handler({});
  }
}

void CatalogueSessionController::request(
    const orc::teletext_wire::ViewSelection& selection, bool show_pending) {
  if (!dialog_ || !session_) {
    return;
  }
  if (show_pending) {
    dialog_->showPending();
  }
  // No reply is delivered once the session is closed, which the host does
  // before the dialog goes; the guard covers the controller going first.
  QPointer<CatalogueSessionController> self(this);
  session_->request(orc::teletext_wire::kCatalogueVerb,
                    orc::teletext_wire::encode_view_selection(selection),
                    [self](orc::view::ToolReply reply) {
                      if (self) {
                        self->onReply(std::move(reply));
                      }
                    });
}

orc::teletext_wire::ViewSelection CatalogueSessionController::currentSelection()
    const {
  orc::teletext_wire::ViewSelection selection;
  if (!dialog_) {
    return selection;
  }
  selection.view_option = dialog_->currentViewOption().toStdString();
  for (const QString& id : dialog_->viewToggles()) {
    if (dialog_->isToggleActive(id)) {
      selection.active_toggles.push_back(id.toStdString());
    }
  }
  return selection;
}

void CatalogueSessionController::onViewSelectionChanged(
    const QString& option_id, const QStringList& active_toggles) {
  orc::teletext_wire::ViewSelection selection;
  selection.view_option = option_id.toStdString();
  for (const QString& id : active_toggles) {
    selection.active_toggles.push_back(id.toStdString());
  }
  request(selection, false);
}

void CatalogueSessionController::onReply(orc::view::ToolReply reply) {
  if (!dialog_) {
    return;
  }
  if (!reply.ok) {
    dialog_->showError(QString::fromStdString(reply.error));
    return;
  }
  orc::CatalogueDataset dataset;
  std::string error;
  if (!orc::teletext_wire::decode_catalogue(reply.data, dataset, &error)) {
    dialog_->showError(tr("The catalogue could not be read: %1")
                           .arg(QString::fromStdString(error)));
    return;
  }
  dialog_->setCatalogue(dataset);
}

namespace {

std::vector<std::string> to_std_strings(const QStringList& list) {
  std::vector<std::string> out;
  out.reserve(static_cast<size_t>(list.size()));
  for (const QString& entry : list) {
    out.push_back(entry.toStdString());
  }
  return out;
}

}  // namespace

void CatalogueSessionController::onKeysPressed(
    const QString& item_id, const QStringList& keys, const QString& option_id,
    const QStringList& active_toggles) {
  if (!session_) {
    return;
  }
  orc::teletext_wire::PlayRequest play;
  play.item_id = item_id.toStdString();
  play.keys = to_std_strings(keys);
  play.selection.view_option = option_id.toStdString();
  play.selection.active_toggles = to_std_strings(active_toggles);

  QPointer<CatalogueSessionController> self(this);
  session_->request(orc::teletext_wire::kPlayedVerb,
                    orc::teletext_wire::encode_play_request(play),
                    [self, item_id, keys, option_id,
                     active_toggles](orc::view::ToolReply reply) {
                      if (!self || !self->dialog_) {
                        return;
                      }
                      // A key that could not be played shows nothing new: the
                      // dialog keeps the page as it stood and says the play
                      // came to nothing.
                      orc::CataloguePayload payload;
                      if (reply.ok) {
                        std::string error;
                        if (!orc::teletext_wire::decode_payload(
                                reply.data, payload, &error)) {
                          payload = orc::CataloguePayload{};
                        }
                      }
                      self->dialog_->setPlayedPayload(item_id, keys, option_id,
                                                      active_toggles, payload);
                    });
}

void CatalogueSessionController::onExportDataRequested(
    const QString& directory) {
  if (!session_) {
    return;
  }
  orc::teletext_wire::ExportRequest request;
  request.directory = directory.toStdString();

  QPointer<CatalogueSessionController> self(this);
  session_->request(
      orc::teletext_wire::kExportVerb,
      orc::teletext_wire::encode_export_request(request),
      [self](orc::view::ToolReply reply) {
        if (!self || !self->dialog_) {
          return;
        }
        if (!reply.ok) {
          self->dialog_->finishExport(std::nullopt,
                                      QString::fromStdString(reply.error));
          return;
        }
        orc::CatalogueExportResult result;
        std::string error;
        if (!orc::teletext_wire::decode_export_result(reply.data, result,
                                                      &error)) {
          self->dialog_->finishExport(
              std::nullopt, tr("The stage's answer could not be read: %1")
                                .arg(QString::fromStdString(error)));
          return;
        }
        // supported is false when the stage has nothing to export: not
        // triggered since the project was last built.
        self->dialog_->finishExport(
            result.supported ? std::optional<orc::CatalogueExportResult>(result)
                             : std::nullopt);
      });
}
