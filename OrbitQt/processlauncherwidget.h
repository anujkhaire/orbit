// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PROCESSLAUNCHERWIDGET_H
#define PROCESSLAUNCHERWIDGET_H

#include <QWidget>

#include "DataView.h"

namespace Ui {
class ProcessLauncherWidget;
}

class ProcessLauncherWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ProcessLauncherWidget(QWidget* parent = nullptr);
  ~ProcessLauncherWidget() override;

  void Refresh();
  void SetDataView(DataView* data_view);
  void SetProcessParams();
  void UpdateProcessParams();

 private slots:
  void on_BrowseButton_clicked();

  void on_LaunchButton_clicked();

  void on_checkBoxPause_clicked(bool checked);

  void on_BrowseWorkingDirButton_clicked();

 private:
  Ui::ProcessLauncherWidget* ui;
};

#endif  // PROCESSLAUNCHERWIDGET_H
