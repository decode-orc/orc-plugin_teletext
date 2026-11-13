/*
 * File:        catalogue_session_controller.h
 * Module:      orc-view_teletext
 * Purpose:     Feeds a CatalogueDialog from the stage through a tool session
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef CATALOGUE_SESSION_CONTROLLER_H
#define CATALOGUE_SESSION_CONTROLLER_H

#include <orc/view/stage_tool_session.h>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>

#include "catalogue/catalogue_wire.h"

class CatalogueDialog;

/**
 * @brief Carries the dialog's questions to the stage and the answers back
 *
 * - On construction the dialog shows "Decoding…" and the catalogue is asked
 *   for under the settings' own presentation.
 * - When the reader picks another view option or toggle, the same catalogue is
 *   asked for under the new selection. What is on screen stays until it
 *   arrives: nothing upstream runs, so there is no decode to wait for.
 * - When the host says the stage was triggered again, the catalogue is asked
 *   for again under the reader's current selection.
 * - A key pressed on an item that takes keys asks the stage what the item
 *   shows after every key pressed on it (the played verb). The dialog drops an
 *   answer that a later key, another item or another view has overtaken.
 * - Export All: the dialog writes what it shows, then the stage writes what
 *   it decoded under the folder (the export verb).
 *
 * Replies arrive on the GUI thread in request order, so the last one asked
 * for is the last one shown. A reply that is an error, or bytes this view
 * cannot read, is shown in place of the catalogue.
 *
 * Created as a child of the dialog, so it goes with it.
 */
class CatalogueSessionController : public QObject {
  Q_OBJECT

 public:
  CatalogueSessionController(
      CatalogueDialog* dialog,
      std::shared_ptr<orc::view::IStageToolSession> session);
  ~CatalogueSessionController() override;

  /// Ask for the catalogue under @p selection. Shows "Decoding…" first when
  /// @p show_pending is set.
  void request(const orc::teletext_wire::ViewSelection& selection,
               bool show_pending);

  /// The reader's current selection, read back from the dialog.
  orc::teletext_wire::ViewSelection currentSelection() const;

 private:
  void onViewSelectionChanged(const QString& option_id,
                              const QStringList& active_toggles);
  void onReply(orc::view::ToolReply reply);
  void onKeysPressed(const QString& item_id, const QStringList& keys,
                     const QString& option_id,
                     const QStringList& active_toggles);
  void onExportDataRequested(const QString& directory);

  QPointer<CatalogueDialog> dialog_;
  std::shared_ptr<orc::view::IStageToolSession> session_;
};

#endif  // CATALOGUE_SESSION_CONTROLLER_H
