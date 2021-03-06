/****************************************************************************
 *   Copyright (c) 2020 Zheng, Lei (realthunder) <realthunder.dev@gmail.com>*
 *                                                                          *
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
#endif

#include <App/Material.h>
#include "DlgSettingsDrawStyles.h"
#include "ui_DlgSettingsDrawStyles.h"
#include "ViewParams.h"
#include "Application.h"
#include "Document.h"
#include "View3DInventorViewer.h"
#include "View3DInventor.h"
#include "PrefWidgets.h"

using namespace Gui::Dialog;

/* TRANSLATOR Gui::Dialog::DlgSettingsDrawStyles */

/**
 *  Constructs a DlgSettingsDrawStyles which is a child of 'parent', with the 
 *  name 'name' and widget flags set to 'f' 
 */
DlgSettingsDrawStyles::DlgSettingsDrawStyles(QWidget* parent)
    : PreferencePage(parent)
    , ui(new Ui_DlgSettingsDrawStyles)
{
    ui->setupUi(this);
    ui->LineColor->setEnabled(ui->checkBoxLineColor->isChecked());
    ui->FaceColor->setEnabled(ui->checkBoxFaceColor->isChecked());

    ui->checkBoxShaded->setChecked(ViewParams::getHiddenLineShaded());
    ui->checkBoxLineColor->setChecked(ViewParams::getHiddenLineOverrideColor());
    ui->checkBoxFaceColor->setChecked(ViewParams::getHiddenLineOverrideFaceColor());

    ui->LineColor->setColor(App::Color(
                (uint32_t)ViewParams::getHiddenLineColor()).asValue<QColor>());

    ui->FaceColor->setColor(App::Color(
                (uint32_t)ViewParams::getHiddenLineFaceColor()).asValue<QColor>());
}

/** 
 *  Destroys the object and frees any allocated resources
 */
DlgSettingsDrawStyles::~DlgSettingsDrawStyles()
{
    // no need to delete child widgets, Qt does it all for us
}

void DlgSettingsDrawStyles::saveSettings()
{
    ui->checkBoxShaded->onSave();
    ui->checkBoxFaceColor->onSave();
    ui->checkBoxLineColor->onSave();
    ui->FaceColor->onSave();
    ui->LineColor->onSave();
    ui->spinTransparency->onSave();

    for(auto doc : App::GetApplication().getDocuments()) {
        for(auto v : Application::Instance->getDocument(doc)->getMDIViews()) {
            View3DInventor* view = qobject_cast<View3DInventor*>(v);
            if(!view)
                continue;
            auto viewer = view->getViewer();
            std::string mode = viewer->getOverrideMode();
            if(mode.size()) {
                viewer->setOverrideMode(std::string());
                viewer->setOverrideMode(mode);
            }
        }
    }
}

void DlgSettingsDrawStyles::loadSettings()
{
    ui->checkBoxShaded->onRestore();
    ui->checkBoxFaceColor->onRestore();
    ui->checkBoxLineColor->onRestore();
    ui->FaceColor->onRestore();
    ui->LineColor->onRestore();
    ui->spinTransparency->onRestore();
}

/**
 * Sets the strings of the subwidgets using the current language.
 */
void DlgSettingsDrawStyles::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    else {
        QWidget::changeEvent(e);
    }
}

#include "moc_DlgSettingsDrawStyles.cpp"

