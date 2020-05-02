/***************************************************************************
 *   Copyright (c) 2002 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
# include <QVBoxLayout>
# include <QTreeWidget>
# include <QTreeWidgetItem>
# include <QLineEdit>
# include <QTextStream>
# include <QToolButton>
# include <QCheckBox>
# include <QMenu>
# include <QLabel>
# include <QApplication>
# include <QHeaderView>
#endif

#include <QHelpEvent>
#include <QToolTip>

/// Here the FreeCAD includes sorted by Base,App,Gui......
#include <Base/Console.h>
#include <App/Document.h>
#include <App/GeoFeature.h>
#include <App/DocumentObserver.h>
#include "SelectionView.h"
#include "CommandT.h"
#include "Application.h"
#include "Document.h"
#include "ViewProvider.h"
#include "BitmapFactory.h"
#include "MetaTypes.h"

FC_LOG_LEVEL_INIT("Selection",true,true,true)

using namespace Gui;
using namespace Gui::DockWnd;

/* TRANSLATOR Gui::DockWnd::SelectionView */

enum ColumnIndex {
    LabelIndex,
    ElementIndex,
    PathIndex,
    LastIndex,
};

SelectionView::SelectionView(Gui::Document* pcDocument, QWidget *parent)
  : DockWindow(pcDocument,parent)
  , SelectionObserver(false,0)
{
    setWindowTitle(tr("Selection View"));

    QVBoxLayout* vLayout = new QVBoxLayout(this);
    vLayout->setSpacing(0);
    vLayout->setMargin (0);

    QLineEdit* searchBox = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    searchBox->setPlaceholderText(tr("Search"));
#endif
    searchBox->setToolTip(tr("Searches object labels"));
    QHBoxLayout* hLayout = new QHBoxLayout();
    hLayout->setSpacing(2);
    QToolButton* clearButton = new QToolButton(this);
    clearButton->setFixedSize(18, 21);
    clearButton->setCursor(Qt::ArrowCursor);
    clearButton->setStyleSheet(QString::fromUtf8("QToolButton {margin-bottom:1px}"));
    clearButton->setIcon(BitmapFactory().pixmap(":/icons/edit-cleartext.svg"));
    clearButton->setToolTip(tr("Clears the search field"));
    clearButton->setAutoRaise(true);
    countLabel = new QLabel(this);
    countLabel->setText(QString::fromUtf8("0"));
    countLabel->setToolTip(tr("The number of selected items"));
    hLayout->addWidget(searchBox);
    hLayout->addWidget(clearButton,0,Qt::AlignRight);
    hLayout->addWidget(countLabel,0,Qt::AlignRight);
    vLayout->addLayout(hLayout);

    selectionView = new QTreeWidget(this);
    selectionView->setColumnCount(LastIndex);
    selectionView->headerItem()->setText(LabelIndex, tr("Label"));
    selectionView->headerItem()->setText(ElementIndex, tr("Element"));
    selectionView->headerItem()->setText(PathIndex, tr("Path"));

    selectionView->setContextMenuPolicy(Qt::CustomContextMenu);
    vLayout->addWidget( selectionView );

    enablePickList = new QCheckBox(this);
    enablePickList->setText(tr("Picked object list"));
    hLayout->addWidget(enablePickList);

    pickList = new QTreeWidget(this);
    pickList->header()->setSortIndicatorShown(true);
    pickList->setColumnCount(LastIndex);
    pickList->sortByColumn(ElementIndex, Qt::AscendingOrder);
    pickList->headerItem()->setText(LabelIndex, tr("Label"));
    pickList->headerItem()->setText(ElementIndex, tr("Element"));
    pickList->headerItem()->setText(PathIndex, tr("Path"));
    pickList->setVisible(false);
    vLayout->addWidget(pickList);

    for(int i=0; i<LastIndex; ++i) {
#if QT_VERSION >= 0x050000
        selectionView->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        pickList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
#else
        selectionView->header()->setResizeMode(i, QHeaderView::ResizeToContents);
        pickList->header()->setResizeMode(i, QHeaderView::ResizeToContents);
#endif
    }

#if QT_VERSION >= 0x040200
    selectionView->setMouseTracking(true); // needed for itemEntered() to work
    pickList->setMouseTracking(true);
#endif

    resize(200, 200);

    connect(clearButton, SIGNAL(clicked()), searchBox, SLOT(clear()));
    connect(searchBox, SIGNAL(textChanged(QString)), this, SLOT(search(QString)));
    connect(searchBox, SIGNAL(editingFinished()), this, SLOT(validateSearch()));
    connect(selectionView, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(toggleSelect(QTreeWidgetItem*)));
    connect(selectionView, SIGNAL(itemEntered(QTreeWidgetItem*, int)), this, SLOT(preselect(QTreeWidgetItem*)));
    connect(pickList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(toggleSelect(QTreeWidgetItem*)));
    connect(pickList, SIGNAL(itemEntered(QTreeWidgetItem*, int)), this, SLOT(preselect(QTreeWidgetItem*)));
    connect(selectionView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onItemContextMenu(QPoint)));
    connect(enablePickList, SIGNAL(stateChanged(int)), this, SLOT(onEnablePickList()));

}

SelectionView::~SelectionView()
{
}

void SelectionView::leaveEvent(QEvent *)
{
    Selection().rmvPreselect();
}

static void addItem(QTreeWidget *tree, const App::SubObjectT &objT)
{
    auto obj = objT.getSubObject();
    if(!obj)
        return;

    auto* item = new QTreeWidgetItem(tree);
    item->setText(LabelIndex, QString::fromUtf8(obj->Label.getStrValue().c_str()));

    item->setText(ElementIndex, QString::fromLatin1(objT.getOldElementName().c_str()));

    item->setText(PathIndex, QString::fromLatin1("%1#%2.%3").arg(
                QString::fromLatin1(objT.getDocumentName().c_str()),
                QString::fromLatin1(objT.getObjectName().c_str()),
                QString::fromLatin1(objT.getSubName().c_str())));

    auto vp = Application::Instance->getViewProvider(obj);
    if(vp)
        item->setIcon(0, vp->getIcon());

    // save as user data
    item->setData(0, Qt::UserRole, QVariant::fromValue(objT));
}

/// @cond DOXERR
void SelectionView::onSelectionChanged(const SelectionChanges &Reason)
{
    QString selObject;
    QTextStream str(&selObject);
    if (Reason.Type == SelectionChanges::AddSelection) {
        addItem(selectionView, Reason.Object);
    }
    else if (Reason.Type == SelectionChanges::ClrSelection) {
        if(!Reason.pDocName[0]) {
            // remove all items
            selectionView->clear();
        }else{
            // build name
            str << Reason.pDocName;
            str << "#";
            // remove all items
            for(auto item : selectionView->findItems(selObject,Qt::MatchStartsWith,PathIndex))
                delete item;
        }
    }
    else if (Reason.Type == SelectionChanges::RmvSelection) {
        // build name
        str << Reason.pDocName << "#" << Reason.pObjectName << "." << Reason.pSubName;

        // remove all items
        for(auto item : selectionView->findItems(selObject,Qt::MatchExactly,PathIndex))
            delete item;
    }
    else if (Reason.Type == SelectionChanges::SetSelection) {
        // remove all items
        selectionView->clear();
        for(auto &objT : Gui::Selection().getSelectionT("*",0))
            addItem(selectionView, objT);
    }
    else if (Reason.Type == SelectionChanges::PickedListChanged) {
        bool picking = Selection().needPickedList();
        enablePickList->setChecked(picking);
        pickList->setVisible(picking);
        pickList->clear();
        if(picking) {
            pickList->setSortingEnabled(false);
            for(auto &objT : Selection().getPickedList("*"))
                addItem(pickList, objT);
            pickList->setSortingEnabled(true);
        }
    }

    countLabel->setText(QString::number(selectionView->topLevelItemCount()));
}

void SelectionView::search(const QString& text)
{
    if (!text.isEmpty()) {
        searchList.clear();
        App::Document* doc = App::GetApplication().getActiveDocument();
        std::vector<App::DocumentObject*> objects;
        if (doc) {
            objects = doc->getObjects();
            selectionView->clear();
            for (std::vector<App::DocumentObject*>::iterator it = objects.begin(); it != objects.end(); ++it) {
                QString label = QString::fromUtf8((*it)->Label.getValue());
                if (label.contains(text,Qt::CaseInsensitive)) {
                    searchList.push_back(*it);
                    addItem(selectionView, App::SubObjectT(*it, ""));
                }
            }
            countLabel->setText(QString::number(selectionView->topLevelItemCount()));
        }
    }
}

void SelectionView::validateSearch(void)
{
    if (!searchList.empty()) {
        App::Document* doc = App::GetApplication().getActiveDocument();
        if (doc) {
            Gui::Selection().clearSelection();
            for (std::vector<App::DocumentObject*>::iterator it = searchList.begin(); it != searchList.end(); ++it) {
                Gui::Selection().addSelection(doc->getName(),(*it)->getNameInDocument(),0);
            }
        }
    }
}

void SelectionView::select(QTreeWidgetItem* item)
{
    if (!item)
        item = selectionView->currentItem();
    if (!item)
        return;

    auto objT = qvariant_cast<App::SubObjectT>(item->data(0, Qt::UserRole));
    if(!objT.getSubObject())
        return;

    try {
        doCommandT(Command::Gui, "Gui.Selection.clearSelection()");
        doCommandT(Command::Gui, "Gui.Selection.addSelection(%s, '%s')",
                Command::getObjectCmd(objT.getObject()), objT.getSubName());
    }catch(Base::Exception &e) {
        e.ReportException();
    }
}

void SelectionView::deselect(void)
{
    auto item = selectionView->currentItem();
    if (!item)
        return;
    auto objT = qvariant_cast<App::SubObjectT>(item->data(0, Qt::UserRole));
    if(!objT.getSubObject())
        return;

    try {
        doCommandT(Command::Gui, "Gui.Selection.removeSelection(%s, '%s')",
                Command::getObjectCmd(objT.getObject()), objT.getSubName());
    }catch(Base::Exception &e) {
        e.ReportException();
    }
}

void SelectionView::toggleSelect(QTreeWidgetItem* item)
{
    if (!item) return;

    auto objT = qvariant_cast<App::SubObjectT>(item->data(0, Qt::UserRole));
    if(!objT.getSubObject())
        return;

    try {
        bool selected = Gui::Selection().isSelected(objT.getDocumentName().c_str(),
                                                    objT.getObjectName().c_str(),
                                                    objT.getSubName().c_str());
        doCommandT(Command::Gui, "Gui.Selection.%s(%s,'%s')",
                selected ? "removeSelection" : "addSelection",
                Command::getObjectCmd(objT.getObject()),
                objT.getSubName());

    }catch(Base::Exception &e) {
        e.ReportException();
    }
}

void SelectionView::preselect(QTreeWidgetItem* item)
{
    if (!item) return;
    auto objT = qvariant_cast<App::SubObjectT>(item->data(0, Qt::UserRole));
    if(!objT.getSubObject())
        return;
    Gui::Selection().setPreselect(objT.getDocumentName().c_str(),
                                  objT.getObjectName().c_str(),
                                  objT.getSubName().c_str(),0,0,0,2);
}

void SelectionView::zoom(void)
{
    select();
    try {
        Gui::Command::runCommand(Gui::Command::Gui,"Gui.SendMsgToActiveView(\"ViewSelection\")");
    }catch(Base::Exception &e) {
        e.ReportException();
    }
}

void SelectionView::treeSelect(void)
{
    select();
    try {
        Gui::Command::runCommand(Gui::Command::Gui,"Gui.runCommand(\"Std_TreeSelection\")");
    }catch(Base::Exception &e) {
        e.ReportException();
    }
}

void SelectionView::touch(void)
{
    auto item = selectionView->currentItem();
    if (!item)
        return;
    auto objT = qvariant_cast<App::SubObjectT>(item->data(0, Qt::UserRole));
    auto sobj = objT.getSubObject();
    if(!sobj)
        return;
    if(sobj) {
        try {
            cmdAppObject(sobj,"touch()");
        }catch(Base::Exception &e) {
            e.ReportException();
        }
    }
}

void SelectionView::toPython(void)
{
    auto item = selectionView->currentItem();
    if (!item)
        return;
    auto objT = qvariant_cast<App::SubObjectT>(item->data(0, Qt::UserRole));
    auto sobj = objT.getSubObject();
    if(!sobj)
        return;
    try {
        doCommandT(Command::Gui, "_obj, _matrix, _shp = %s.getSubObject('%s', retType=2)",
                Command::getObjectCmd(objT.getObject()), objT.getSubName());
    }
    catch (const Base::Exception& e) {
        e.ReportException();
    }
}

static std::string getModule(const char* type)
{
    // go up the inheritance tree and find the module name of the first
    // sub-class that has not the prefix "App::"
    std::string prefix;
    Base::Type typeId = Base::Type::fromName(type);

    while (!typeId.isBad()) {
        std::string temp(typeId.getName());
        std::string::size_type pos = temp.find_first_of("::");

        std::string module;
        if (pos != std::string::npos)
            module = std::string(temp,0,pos);
        if (module != "App")
            prefix = module;
        else
            break;
        typeId = typeId.getParent();
    }

    return prefix;
}

void SelectionView::showPart(void)
{
    auto *item = selectionView->currentItem();
    if (!item)
        return;
    auto objT = qvariant_cast<App::SubObjectT>(item->data(0, Qt::UserRole));
    auto sobj = objT.getSubObject();
    if(!sobj)
        return;
    std::string module = getModule(sobj->getTypeId().getName());
    if (!module.empty()) {
        try {
            doCommandT(Command::Gui, "%s.show(%s.getSubObject('%s'))",
                    module, Command::getObjectCmd(objT.getObject()), objT.getSubName());
        }
        catch (const Base::Exception& e) {
            e.ReportException();
        }
    }
}

void SelectionView::onItemContextMenu(const QPoint& point)
{
    auto item = selectionView->itemAt(point);
    if (!item)
        return;
    auto objT = qvariant_cast<App::SubObjectT>(item->data(0, Qt::UserRole));
    auto sobj = objT.getSubObject();
    if(!sobj)
        return;

    QMenu menu;
    QAction *selectAction = menu.addAction(tr("Select only"),this,SLOT(select()));
    selectAction->setIcon(QIcon::fromTheme(QString::fromLatin1("view-select")));
    selectAction->setToolTip(tr("Selects only this object"));
    QAction *deselectAction = menu.addAction(tr("Deselect"),this,SLOT(deselect()));
    deselectAction->setIcon(QIcon::fromTheme(QString::fromLatin1("view-unselectable")));
    deselectAction->setToolTip(tr("Deselects this object"));
    QAction *zoomAction = menu.addAction(tr("Zoom fit"),this,SLOT(zoom()));
    zoomAction->setIcon(QIcon::fromTheme(QString::fromLatin1("zoom-fit-best")));
    zoomAction->setToolTip(tr("Selects and fits this object in the 3D window"));
    QAction *gotoAction = menu.addAction(tr("Go to selection"),this,SLOT(treeSelect()));
    gotoAction->setToolTip(tr("Selects and locates this object in the tree view"));
    QAction *touchAction = menu.addAction(tr("Mark to recompute"),this,SLOT(touch()));
    touchAction->setIcon(QIcon::fromTheme(QString::fromLatin1("view-refresh")));
    touchAction->setToolTip(tr("Mark this object to be recomputed"));
    QAction *toPythonAction = menu.addAction(tr("To python console"),this,SLOT(toPython()));
    toPythonAction->setIcon(QIcon::fromTheme(QString::fromLatin1("applications-python")));
    toPythonAction->setToolTip(tr("Reveals this object and its subelements in the python console."));

    if (objT.getOldElementName().size()) {
        // subshape-specific entries
        QAction *showPart = menu.addAction(tr("Duplicate subshape"),this,SLOT(showPart()));
        showPart->setIcon(QIcon(QString::fromLatin1(":/icons/ClassBrowser/member.svg")));
        showPart->setToolTip(tr("Creates a standalone copy of this subshape in the document"));
    }
    menu.exec(selectionView->mapToGlobal(point));
}

void SelectionView::onUpdate(void)
{
}

bool SelectionView::onMsg(const char* /*pMsg*/,const char** /*ppReturn*/)
{
    return false;
}

void SelectionView::hideEvent(QHideEvent *ev) {
    FC_TRACE(this << " detaching selection observer");
    this->detachSelection();
    DockWindow::hideEvent(ev);
}

void SelectionView::showEvent(QShowEvent *ev) {
    FC_TRACE(this << " attaching selection observer");
    this->attachSelection();
    enablePickList->setChecked(Selection().needPickedList());
    Gui::DockWindow::showEvent(ev);
}

void SelectionView::onEnablePickList() {
    bool enabled = enablePickList->isChecked();
    Selection().enablePickedList(enabled);
    pickList->setVisible(enabled);
}

/// @endcond

////////////////////////////////////////////////////////////////////////

SelectionMenu::SelectionMenu(QWidget *parent)
    :QMenu(parent),pSelList(0)
{}

struct ElementInfo {
    QMenu *menu = nullptr;
    std::vector<int> indices;
};

struct SubMenuInfo {
    QMenu *menu = nullptr;

    // Map from sub-object label to map from object path to element info The
    // reason of the second map is to disambiguate sub-object with the same
    // label, but different object or object path
    std::map<std::string, std::map<std::string, ElementInfo> > items;
};

void SelectionMenu::doPick(const std::vector<App::SubObjectT> &sels) {
    clear();
    setStyleSheet(QLatin1String("* { menu-scrollable: 1 }"));

    pSelList = &sels;
    std::ostringstream ss;
    std::map<std::string, SubMenuInfo> menus;

    int i=0;
    for(auto &sel : sels) {
        auto sobj = sel.getSubObject();
        if(!sobj)
            continue;

        ss.str("");
        int index = -1;
        std::string element = sel.getOldElementName(&index);
        if(index < 0)
            continue;
        ss << sel.getObjectName() << '.' << sel.getSubNameNoElement();
        std::string key = ss.str();

        menus[element].items[sobj->Label.getStrValue()][key].indices.push_back(i++);
    }

    for(auto &v : menus) {
        auto &info = v.second;
        info.menu = addMenu(QLatin1String(v.first.c_str()));

        for(auto &vv : info.items) {
            const std::string &label = vv.first;

            for(auto &vvv : vv.second) {
                auto &elementInfo = vvv.second;

                if(sels.size() <= 20) {
                    for(int idx : elementInfo.indices) {
                        ss.str("");
                        ss << label << " (" << sels[idx].getOldElementName() << ")";
                        QAction *action = info.menu->addAction(QString::fromUtf8(ss.str().c_str()));
                        action->setData(idx);
                    }
                    continue;
                }
                if(!elementInfo.menu) {
                    elementInfo.menu = info.menu->addMenu(QString::fromUtf8(label.c_str()));
                    connect(elementInfo.menu, SIGNAL(aboutToShow()),this,SLOT(onSubMenu()));
                }
                for(int idx : elementInfo.indices) {
                    QAction *action = elementInfo.menu->addAction(
                            QString::fromUtf8(sels[idx].getOldElementName().c_str()));
                    action->setData(idx);
                }
            }
        }
    }
    bool toggle = !Gui::Selection().needPickedList();
    if(toggle)
        Gui::Selection().enablePickedList(true);

    Gui::Selection().rmvPreselect();

    timer.setSingleShot(true);
    connect(&timer, SIGNAL(timeout()), this, SLOT(onTimer()));

    connect(this, SIGNAL(hovered(QAction*)), this, SLOT(onHover(QAction*)));
    QAction* picked = exec(QCursor::pos());

    timer.stop();
    QToolTip::hideText();

    if(picked) {
        int idx = picked->data().toInt();
        auto &sel = sels[idx];

        bool ctrl = (QApplication::queryKeyboardModifiers() == Qt::ControlModifier);
        if(!ctrl) {
            if(TreeParams::Instance()->RecordSelection())
                Gui::Selection().selStackPush();
            Gui::Selection().clearSelection();
        }
        Gui::Selection().addSelection(sel.getDocumentName().c_str(),
                sel.getObjectName().c_str(), sel.getSubName().c_str());
        if(TreeParams::Instance()->RecordSelection())
            Gui::Selection().selStackPush(false,ctrl);
    }
    pSelList = 0;
    if(toggle)
        Gui::Selection().enablePickedList(false);
}

void SelectionMenu::onHover(QAction *action) {
    timer.stop();
    QToolTip::hideText();

    if(!pSelList)
        return;
    int idx = action->data().toInt();
    if(idx<0 || idx>=(int)pSelList->size())
        return;
    auto &sel = (*pSelList)[idx];
    Gui::Selection().setPreselect(sel.getDocumentName().c_str(),
            sel.getObjectName().c_str(), sel.getSubName().c_str(),0,0,0,2);
    timer.start(500);
    tooltipIndex = idx;
}

void SelectionMenu::leaveEvent(QEvent *event) {
    timer.stop();
    QToolTip::hideText();
    QMenu::leaveEvent(event);
}

void SelectionMenu::onTimer() {
    bool needElement = tooltipIndex < 0;
    if(needElement)
        tooltipIndex = -tooltipIndex - 1;

    auto &sel = (*pSelList)[tooltipIndex];
    auto sobj = sel.getSubObject();
    QString tooltip;

    QString element;
    if(needElement)
        element = QString::fromLatin1(sel.getOldElementName().c_str());

    if(sobj)
        tooltip = QString::fromLatin1("%1 (%2.%3%4)").arg(
                        QString::fromUtf8(sobj->Label.getValue()),
                        QString::fromLatin1(sel.getObjectName().c_str()),
                        QString::fromLatin1(sel.getSubNameNoElement().c_str()),
                        element);
    else
        tooltip = QString::fromLatin1("%1.%2%3").arg(
                        QString::fromLatin1(sel.getObjectName().c_str()),
                        QString::fromLatin1(sel.getSubNameNoElement().c_str()),
                        element);
    QToolTip::showText(QCursor::pos(), tooltip);
}

void SelectionMenu::onSubMenu() {
    timer.stop();
    QToolTip::hideText();

    auto submenu = qobject_cast<QMenu*>(sender());
    if(!submenu)
        return;
    auto actions = submenu->actions();
    if(!actions.size())
        return;
    int idx = actions.front()->data().toInt();
    if(idx<0 || idx>=(int)pSelList->size())
        return;
    auto &sel = (*pSelList)[idx];

    const char *element = sel.getElementName();
    std::string subname(sel.getSubName().c_str(),element);

    Gui::Selection().setPreselect(sel.getDocumentName().c_str(),
            sel.getObjectName().c_str(), subname.c_str(),0,0,0,2);

    timer.start(500);
    tooltipIndex = -idx-1;
}

#include "moc_SelectionView.cpp"
