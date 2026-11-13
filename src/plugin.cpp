/*
 * File:        plugin.cpp
 * Module:      orc-stage-plugin-teletext
 * Purpose:     Plugin entrypoints: registers teletext_sink and nabts_sink
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "plugin.h"

#include "nabts_sink_stage.h"
#include "teletext_sink_stage.h"

ORC_DEFINE_STAGE_PLUGIN(orc::plugins::teletext::kPluginDescriptor,
                        orc::TeletextSinkStage, orc::NabtsSinkStage)
