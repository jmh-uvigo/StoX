/********************************************************************************************
 *                                                                                          *
 *  StoX                                                                                    *
 *  ----                                                                                    *
 *                                                                                          *
 *  Copyright 2008-2024 J.Martín-Herrero (Universiy of Vigo, Spain).                        *
 *                        julio@uvigo.es                                                    *
 *                                                                                          *
 *  This file is part of StoX.                                                              *
 *                                                                                          *
 *  StoX is free software: you can redistribute it and/or modify it under the terms of      *
 *  the GNU General Public License as published by the Free Software Foundation, either     *
 *  version 3 of the License, or any later version.                                         *
 *                                                                                          *
 *  StoX is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;       *
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR        *
 *  PURPOSE. See the GNU General Public License for more details.                           *
 *                                                                                          *
 *  You should have received a copy of the GNU General Public License along with StoX.      *
 *  If not, see <https://www.gnu.org/licenses/>.                                            *
 *                                                                                          *
 *                                                                                          *
 ********************************************************************************************/


#include "stox.h"
#include "./ui_stox.h"

#include <QMessageBox>
#include <QDateTime>
#include <QTextDocumentWriter>
#include <QFileDialog>
#include <QClipboard>
#include <QMimeData>
#include <QTextTable>
#include <random>


Stox::Stox(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Stox)
{
    // Set up the user interface
    ui->setupUi(this);

    // Prepare the info panels in the status bar
    LSave=new QLabel;
    LSave->setAlignment(Qt::AlignHCenter);
    LSave->setMinimumWidth(72);
    LCheck=new QLabel;
    LCheck->setAlignment(Qt::AlignHCenter);
    LCheck->setMinimumWidth(72);
    ui->statusbar->addPermanentWidget(LCheck,0);
    ui->statusbar->addPermanentWidget(LSave,0);

    // Hide the button that interrupts the model run
    ui->BCancel->hide();

    // Restore parameteres from the last session
    QFile file("Stox.ini");
    if (file.open(QIODevice::ReadOnly)) {
        QDataStream stream(&file);
        stream>>Path;   // User folder from last session
        QString str;
        stream>>str; ui->EIters->setText(str);  // Iterations from last session
        stream>>str; ui->EInitial->setText(str);// Init. nr.    "    "     "
        stream>>str; ui->EEps->setText(str);    // Epsilon      "    "     "
        file.close();
    }

    // Init global flags and variables
    NumTables=0;
    Output=nullptr;

    setChecked(false);  // Model not checked yet
    Saved=true;         // Model does not need saving yet
    LSave->setText(" ");
    FileName.clear();   // No file name for the model yet
    Path="..";

    CloneMode=false;    // Not in stage replication mode
    SourceClone=nullptr;  // No stage to replicate

    NodeType=0; // Direct transit (100% of the seeds go to the next stage)
    TypeNames<<"Direct"<<"Caster"<<"Success"<<"Sink";

    // Set up the model tree view
    ui->TreeWid->setColumnCount(4);
    ui->TreeWid->setHeaderLabels(QStringList() << "Stage" << "Casting" << "Report" << "ID");
    ui->TreeWid->header()->setStretchLastSection(false);
    ui->TreeWid->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->TreeWid->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->TreeWid->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->TreeWid->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    // The first node is always the same
    ui->TreeWid->addTopLevelItem(new QTreeWidgetItem(static_cast<QTreeWidget *>(nullptr), QStringList() << "Start"));

    // Set up the output view
    QHeaderView *vh=ui->TVOutput->verticalHeader();
    vh->setSectionResizeMode(QHeaderView::Fixed);
    vh->setMinimumSectionSize(-1);
    vh->setDefaultSectionSize(12);

    // Set up the casting list
    ui->CBCastings->setInsertPolicy(QComboBox::InsertAlphabetically);

    // Properly seed the Mersenne Twister pseudorandom generator
    generator = new std::mt19937(rand_dev()^
                                 ((std::mt19937::result_type)std::chrono::duration_cast<std::chrono::seconds>
                                  (std::chrono::system_clock::now().time_since_epoch()).count()+
                                  (std::mt19937::result_type)std::chrono::duration_cast<std::chrono::microseconds>
                                  (std::chrono::high_resolution_clock::now().time_since_epoch()).count()));

}

// Destructor
Stox::~Stox()
{
    // Program ends: Tidy up
    delete ui;

    for(auto &&t: Tables) delete t;
    if(Output) delete Output;

}

// Status bar indicator: model has been checked for consistency
void Stox::setChecked(bool stat) {
    if(Checked!=stat) {
        Checked=stat;
        LCheck->setText(stat?"Checked":"Not checked");
    }
}

// Status bar indicator: model has been saved/modified
void Stox::setSaved(bool stat) {
    Saved=stat;
    LSave->setText(stat?"Saved":"Modified");
}

// Add an stage to the model tree
void Stox::AddTreeChild(QTreeWidgetItem *parent, QString name, QString cast, bool show)
{
    QTreeWidgetItem *treeItem = new QTreeWidgetItem();

    // Stage name
    treeItem->setText(0, name);
    // Stage type / associated casting table
    treeItem->setText(1, cast);
    // Whether the stage is reported in the model output
    treeItem->setCheckState(2,show?Qt::Checked:Qt::Unchecked);

    parent->addChild(treeItem);
}


// Create an empty casting table
void Stox::on_BNewTable_clicked()
{

    if(ui->ENewCastName->text().isEmpty()) {
        ui->statusbar->showMessage("Create casting: Input a new name and try again.",5000);
        ui->ENewCastName->setFocus();
        return;
    }

    QString name=ui->ENewCastName->text();
    if(TypeNames.contains(name)) {
        ui->statusbar->showMessage("Create casting: '"+name+"' is not allowed as a casting name. Try another one.",5000);
        ui->ENewCastName->clear();
        ui->ENewCastName->setFocus();
        return;
    }

    int ind=ui->CBCastings->findText(name);
    if(ind>=0) {
        ui->CBCastings->setCurrentIndex(ind);
        ui->statusbar->showMessage("Create casting: Casting '"+name+"' already exists.",5000);
        ui->ENewCastName->clear();
        ui->ENewCastName->setFocus();
        return;
    }


    ui->ENewCastName->clear();

    TableModel *dummy=new TableModel;
    Tables.push_back(dummy);
    NumTables++;

    dummy->FillZeroes(ui->SBRows->value(),ui->SBCols->value(),name);

    ui->CBCastings->addItem(name);
    ui->CBCastings->model()->sort(0);
    ind=ui->CBCastings->findText(name);
    ui->CBCastings->setCurrentIndex(ind);

    setChecked(false);
    setSaved(false);

}

// Duplicate the current casting table
void Stox::on_BDupliTable_clicked()
{
    if(ui->CBCastings->currentIndex()<0) {
        ui->statusbar->showMessage("Duplicate casting: There is no casting currently selected.",5000);
        return;
    }

    if(ui->ENewCastName->text().isEmpty()) {
        ui->statusbar->showMessage("Duplicate casting: Input a new name and try again.",5000);
        ui->ENewCastName->setFocus();
        return;
    }

    QString name=ui->ENewCastName->text();
    if(TypeNames.contains(name)) {
        ui->statusbar->showMessage("Duplicate casting: '"+name+"' is not allowed as a casting name. Try another one.",5000);
        ui->ENewCastName->clear();
        ui->ENewCastName->setFocus();
        return;
    }

    int ind=ui->CBCastings->findText(name);
    if(ind>=0) {
        ui->CBCastings->setCurrentIndex(ind);
        ui->statusbar->showMessage("Duplicate casting: Casting '"+name+"' already exists.",5000);
        ui->ENewCastName->clear();
        ui->ENewCastName->setFocus();
        return;
    }

    ui->ENewCastName->clear();

    TableModel *dummy=new TableModel;
    Tables.push_back(dummy);
    NumTables++;

    dummy->FillFromCopy(static_cast<TableModel*>(ui->TableView->model()),name);

    ui->CBCastings->addItem(name);
    ui->CBCastings->model()->sort(0);
    ind=ui->CBCastings->findText(name);
    ui->CBCastings->setCurrentIndex(ind);

    setChecked(false);
    setSaved(false);
}


// Paste table values (e.g. from MS Excel) into new empty casting
void Stox::on_BPasteTable_clicked()
{
    if(ui->ENewCastName->text().isEmpty()) {
        ui->statusbar->showMessage("Paste casting: Input a new name and try again.",5000);
        ui->ENewCastName->setFocus();
        return;
    }

    QString name=ui->ENewCastName->text();
    if(TypeNames.contains(name)) {
        ui->statusbar->showMessage("Paste casting: '"+name+"' is not allowed as a casting name. Try another one.",5000);
        ui->ENewCastName->clear();
        ui->ENewCastName->setFocus();
        return;
    }

    int ind=ui->CBCastings->findText(name);
    if(ind>=0) {
        ui->CBCastings->setCurrentIndex(ind);
        ui->statusbar->showMessage("Paste casting: Casting '"+name+"' already exists.",5000);
        ui->ENewCastName->clear();
        ui->ENewCastName->setFocus();
        return;
    }

    ui->ENewCastName->clear();

    TableModel *dummy=new TableModel;
    Tables.push_back(dummy);
    NumTables++;

    const QMimeData *mimeData=QApplication::clipboard()->mimeData();
    if(mimeData->hasText()) {
        QString source=mimeData->text();
        // Valid format: Text with columns separated by tabs and lines separated by newline
        int c=source.count("\t");
        int r=source.count("\n");
        if(c<2||r<1) {
            ui->statusbar->showMessage("Paste casting: Clipboard contents not compatible.",5000);
            return;
        }
        c+=r;
        c/=r;
        QTextStream parser(&source);
        float *raw=new float[r*c];
        for(int i=0;i<r*c;++i) parser>>raw[i];
        dummy->FillFromRaw(raw,r,c);
        dummy->setName(name);
        delete [] raw;
    } else {
        ui->statusbar->showMessage("Paste casting: Clipboard contents not compatible.",5000);
        return;
    }

    ui->CBCastings->addItem(name);
    ui->CBCastings->model()->sort(0);
    ind=ui->CBCastings->findText(name);
    ui->CBCastings->setCurrentIndex(ind);

    setChecked(false);
    setSaved(false);
}


// Rename the current casting
void Stox::on_BRenameTable_clicked()
{
    if(ui->CBCastings->currentIndex()<0) {
        ui->statusbar->showMessage("Rename casting: There is no casting currently selected.",5000);
        return;
    }
    if(ui->ENewCastName->text().isEmpty()) {
        ui->ENewCastName->setFocus();
        ui->statusbar->showMessage("Rename casting: Input a new name and try again.",5000);
        return;
    }

    QString name=ui->ENewCastName->text();

    // Chek if name is a reserved word
    if(TypeNames.contains(name)) {
        ui->statusbar->showMessage("'"+name+"' is not allowed as a casting name. Try another one.",5000);
        ui->ENewCastName->clear();
        ui->ENewCastName->setFocus();
        return;
    }

    QString oldname=ui->CBCastings->itemText(ui->CBCastings->currentIndex());

    int ind=ui->CBCastings->findText(name);
    if(ind>=0&&ind!=ui->CBCastings->currentIndex()) {
        ui->statusbar->showMessage("Rename casting: A casting with name '"+name+"' already exists.",5000);
        ui->ENewCastName->clear();
        ui->ENewCastName->setFocus();
        return;
    }

    ui->CBCastings->setItemText(ui->CBCastings->currentIndex(),name);
    ui->CBCastings->model()->sort(0);


    // Change name in underlying table
    static_cast<TableModel*>(ui->TableView->model())->setName(name);


    // Change name in all stages in the tree model using that casting
    QTreeWidgetItemIterator it(ui->TreeWid);
    while(*it) {
        if((*it)->text(1)==oldname) (*it)->setText(1,name);
        ++it;
    }

    setChecked(false);
    setSaved(false);

}


// Delete the current casting
void Stox::on_BDelTable_clicked()
{
    if(ui->CBCastings->currentIndex()<0) {
        ui->statusbar->showMessage("Delete casting: There is no casting currently selected.",5000);
        return;
    }

    QString name=ui->CBCastings->currentText();

    // Count how many stages used that casting
    int c=0;
    QTreeWidgetItemIterator it(ui->TreeWid);
    while(*it) {
        if ((*it)->text(1)==name) c++;
        ++it;
    }

    QMessageBox box;
    if(c) box.setText("Casting '"+name+"' to be deleted is used by "+QString::number(c)+
                    (c==1?" model node. You will need to assign other casting to that node. Do you want to proceed?":
                          " model nodes. You will need to assign other castings to those nodes. Do you want to proceed?"));
    else box.setText("Delete casting '"+name+"': Are you sure?");
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Yes);
    if(box.exec()!=QMessageBox::Yes) return;

    TableModel *remTable=static_cast<TableModel*>(ui->TableView->model());

    ui->CBCastings->removeItem(ui->CBCastings->currentIndex());

    Tables.remove(remTable);
    NumTables--;

    // Remove casting from all stages that had it assigned (if any)
    if(c) {
        it=QTreeWidgetItemIterator(ui->TreeWid);
        while(*it) {
            if((*it)->text(1)==name) (*it)->setText(1,"");
            ++it;
        }
    }

    setChecked(false);
    setSaved(false);

}


// Show the transition table values when a casting is selected in the list
void Stox::on_CBCastings_currentIndexChanged(int index)
{
    // Locate current table from name in CBox, and show it
    QString name=ui->CBCastings->currentText();
    for(auto &&t: Tables) {
        if(t->readName()==name) {
            ui->TableView->setModel(t);
            ui->TableView->resizeColumnsToContents();
            if(t->readCols()<5) ui->TableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            ui->SBRows->setValue(t->readRows());
            ui->SBCols->setValue(t->readCols());
            return;
        }
    }
    // No table with that name (or no tables at all)
    ui->TableView->setModel(nullptr);
}


// Add a stage following the current stage in the model tree
void Stox::on_BChild_clicked()
{
    if(!ui->TreeWid->currentItem()) {
        ui->statusbar->showMessage("Add stage: There is no stage currently selected in the model.",5000);
        return;
    }
    if(ui->ENewNodeName->text().isEmpty()) {
        ui->ENewNodeName->setFocus();
        ui->statusbar->showMessage("Add stage: Input a name for the stage and try again.",5000);
        return;
    }

    AddTreeChild(ui->TreeWid->currentItem(),ui->ENewNodeName->text(),NodeType==1?ui->CBCastings->currentText():TypeNames[NodeType],0);
    ui->ENewNodeName->clear();

    setChecked(false);
    setSaved(false);

}


// Add a stage at the same level of the current stage in the model tree
void Stox::on_BSibling_clicked()
{
    QTreeWidgetItem *item=ui->TreeWid->currentItem();
    if(!item) {
        ui->statusbar->showMessage("Add stage: There is no stage currently selected in the model.",5000);
        return;
    }
    if(!item->parent()) {
        ui->statusbar->showMessage("Add stage: No stage can be added at Start stage side. Use 'Add under' instead, or select another stage in the tree.",5000);
        return;
    }
    if(ui->ENewNodeName->text().isEmpty()) {
        ui->ENewNodeName->setFocus();
        ui->statusbar->showMessage("Add stage: Input a name for the stage and try again.",5000);
        return;
    }

    AddTreeChild(item->parent(),ui->ENewNodeName->text(),NodeType==1?ui->CBCastings->currentText():TypeNames[NodeType],0);
    ui->ENewNodeName->clear();

    setChecked(false);
    setSaved(false);

}


// Change name of current stage
void Stox::on_BRenameStage_clicked()
{
    if(!ui->TreeWid->currentItem()) {
        ui->statusbar->showMessage("Rename stage: There is no stage currently selected in the model.",5000);
        return;
    }
    if(ui->ENewNodeName->text().isEmpty()) {
        ui->ENewNodeName->setFocus();
        ui->statusbar->showMessage("Rename stage: Input a new name and try again.",5000);
        return;
    }

    ui->TreeWid->currentItem()->setText(0,ui->ENewNodeName->text());
    ui->ENewNodeName->clear();

    setChecked(false);
    setSaved(false);

}


// Assign casting table to current stage
void Stox::on_BSetCasting_clicked()
{
    if(!ui->TreeWid->currentItem()) {
        ui->statusbar->showMessage("Set casting: There is no stage currently selected in the model.",5000);
        return;
    }
    if(ui->CBCastings->currentIndex()<0) {
        ui->statusbar->showMessage("Set casting: Please select a casting and try again.",5000);
        return;
    }

    ui->TreeWid->currentItem()->setText(1,ui->CBCastings->currentText());

    setChecked(false);
    setSaved(false);

}


// Delete current stage from the model tree
void Stox::on_BRemoveNode_clicked()
{
    if(CloneMode) {
        ui->statusbar->showMessage("Replication of '"+SourceClone->text(0)+"' in process. To abort replication uncheck 'Replicate' button.",5000);
        return;
    }

    QTreeWidgetItem *item=ui->TreeWid->currentItem();
    if(!item)  {
        ui->statusbar->showMessage("Remove stage: There is no stage currently selected in the model.",5000);
        return;
    }

    QMessageBox box;
    if(item->childCount()>0) box.setText("Remove stage '"+item->text(0)+"': The stage selected has dependent stages. Are you sure?");
    else box.setText("Remove stage '"+item->text(0)+"': Are you sure?");
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Yes);
    if(box.exec()!=QMessageBox::Yes) return;

    RemoveNode(item);

    setChecked(false);
    setSaved(false);

}

// Safely remove current stage from model tree and all following it
void Stox::RemoveNode(QTreeWidgetItem *node)
{
    int C=node->childCount();
    for(int c=0;c<C;++c) RemoveNode(node->child(0));

    QTreeWidgetItem *parentItem=node->parent();
    QTreeWidgetItem *takenItem=parentItem?parentItem->takeChild(parentItem->indexOfChild(node)):ui->TreeWid->takeTopLevelItem(ui->TreeWid->indexOfTopLevelItem(node));
    delete takenItem;
}


// Start stage replication operation
void Stox::on_BCloneNode_clicked()
{
    bool newCloneMode=ui->BCloneNode->isChecked();
    if(CloneMode && !newCloneMode) {
        // Abort clone operation
        ui->TreeWid->unsetCursor();
        ui->statusbar->showMessage("Replicate stage: Replication of '"+SourceClone->text(0)+"' stage has been aborted.",5000);
        CloneMode=false;
        return;
    }
    if(newCloneMode) {
        // Expect user to select a stage where the replication will take place
        ui->TreeWid->setCursor(QCursor(Qt::PointingHandCursor));
        SourceClone=ui->TreeWid->currentItem();
        ui->statusbar->showMessage("Replicate stage: Click stage where replicated '"+SourceClone->text(0)+"' stage should be placed.",5000);
        CloneMode=true;
    }

    setChecked(false);
    setSaved(false);

}

// React to stage selected in the model tree as replication target location
void Stox::on_TreeWid_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if(!current) return;

    // Complete replication if one was pending
    if(CloneMode) {
        current->addChild(SourceClone->clone());
        ui->TreeWid->unsetCursor();
        ui->BCloneNode->setChecked(0);
        CloneMode=false;
        return;
    }
    // If the selected stage does not have a casting assigned, we are done
    if(current->text(1).isEmpty()||current->text(1).isNull()) return;
    // Else show the casting
    int ind=ui->CBCastings->findText(current->text(1));
    if(ind>=0&&ind!=ui->CBCastings->currentIndex()) ui->CBCastings->setCurrentIndex(ind);
}


// Set stage type
void Stox::on_BSetType_clicked()
{
    if(!ui->TreeWid->currentItem()) {
        ui->statusbar->showMessage("Set stage type: There is no stage currently selected in the model.",5000);
        return;
    }

    ui->TreeWid->currentItem()->setText(1,NodeType==1?ui->CBCastings->currentText():TypeNames[NodeType]);

    setChecked(false);
    setSaved(false);
}

// Keep track of the selected type
void Stox::on_RBNodeTypeDirect_toggled(bool checked)
{
    if(checked) NodeType=0;
}

void Stox::on_RBNodeTypeCaster_toggled(bool checked)
{
    if(checked) NodeType=1;
}

void Stox::on_RBNodeTypeSuccess_toggled(bool checked)
{
    if(checked) NodeType=2;
}

void Stox::on_RBNodeTypeSink_toggled(bool checked)
{
    if(checked) NodeType=3;
}


// Check all 'report' checkmarks in the model tree: all stages are reported in the model run output
void Stox::on_BShowAll_clicked()
{
    QTreeWidgetItemIterator it(ui->TreeWid);
    ++it;
    while(*it) {
        (*it)->setCheckState(2,Qt::Checked);
        ++it;
    }
}

// Check all'report' checkmarks of "Success" terminal stages, and unchecked  all the rest
void Stox::on_BShowSuccess_clicked()
{
    QTreeWidgetItemIterator it(ui->TreeWid);
    ++it;
    while(*it) {
        (*it)->setCheckState(2,(*it)->text(1)=="Success"?Qt::Checked:Qt::Unchecked);
        ++it;
    }
}

// Uncheck all 'report' checkmarks in the model tree: no stage is reported in the model run ouput
void Stox::on_BShowNone_clicked()
{
    QTreeWidgetItemIterator it(ui->TreeWid);
    ++it;
    while(*it) {
        (*it)->setCheckState(2,Qt::Unchecked);
        ++it;
    }
}

// Expand the whole tree model
void Stox::on_BExpandTree_clicked()
{
    Xpand(*ui->TreeWid->topLevelItem(0));
}

// Expand the tree model downstream from stage 'item'
void Stox::Xpand(QTreeWidgetItem &item) {
    item.setExpanded(true);
    for(int c=0;c<item.childCount();++c) Xpand(*item.child(c));
}

// Assigns a unique hyerachical ID to each stage in the model tree for identification
void Stox::IDMarkTree()
{
    QTreeWidgetItemIterator it(ui->TreeWid);
    (*it)->setText(3,"1");
    //++it;
    while(*it) {
        int C=(*it)->childCount();
        QString IDp=(*it)->text(3);
        for(int c=0;c<C;) (*it)->child(c)->setText(3,IDp+"."+QString::number(++c));
        ++it;
    }
}

// Check the model for consistency
void Stox::on_actionCheck_triggered()
{
    if(CloneMode) {
        ui->statusbar->showMessage("Replication of '"+SourceClone->text(0)+"' in process. To abort replication uncheck 'Replicate' button.",5000);
        return;
    }

    setChecked(false);

    IDMarkTree();

    // Check coherence of stage types and following stages
    QTreeWidgetItemIterator it(ui->TreeWid);
    while(*it) {
        int n=(*it)->childCount();
        const QString &Casting=(*it)->text(1);
        if(n==0) {
            if(Casting!="Success"&&Casting!="Sink") {
                ui->TreeWid->currentItem()->setSelected(false);
                (*it)->setSelected(true);
                QMessageBox box;
                box.setIcon(QMessageBox::Critical);
                box.setText("Stage '"+(*it)->text(0)+"' ("+(*it)->text(3)+") has no following stages, it should be type 'sink' or type 'success'.");
                box.exec();
                return;
            }
        } else if(n==1) {
            if(Casting!="Direct") {
                ui->TreeWid->currentItem()->setSelected(false);
                (*it)->setSelected(true);
                QMessageBox box;
                box.setIcon(QMessageBox::Critical);
                box.setText("Stage '"+(*it)->text(0)+"' ("+(*it)->text(3)+") has only one following stage, it should be type 'direct'.");
                box.exec();
                return;
            }
        } else {
            if(Casting=="Direct"||Casting=="Success"||Casting=="Sink") {
                ui->TreeWid->currentItem()->setSelected(false);
                (*it)->setSelected(true);
                QMessageBox box;
                box.setIcon(QMessageBox::Critical);
                box.setText("Stage '"+(*it)->text(0)+"' ("+(*it)->text(3)+") has more than one following stage but no casting, it should have a casting set.");
                box.exec();
                return;
            } else {
                // Look for associated casting table
                for(auto &&t: Tables) {
                    if(t->readName()==Casting) {
                        // Check if number of cols matches number of child stages
                        if(t->readCols()!=n) {
                            ui->TreeWid->currentItem()->setSelected(false);
                            (*it)->setSelected(true);
                            QMessageBox box;
                            box.setIcon(QMessageBox::Critical);
                            box.setText("Stage '"+(*it)->text(0)+"' ("+(*it)->text(3)+") has "+QString::number(n)+" following stages but its casting '"+Casting+"' has "+QString::number(t->readCols())+" columns.");
                            box.exec();
                            return;
                        }
                        // Found table and is OK.
                        break;
                    }
                }
            }
        }
        ++it;
    }

    // Warn about castings with rows sums not equal to 1.0 (individuals vanish in thin air)
    int warnings=0;
    for(auto &&t: Tables) {
        int rows=t->readRows();
        for(int r=0;r<rows;++r) {
            float sum=t->sumCols(r);
            if(fabs(1.0-sum)>0.001) {
                warnings++;
                ui->TableView->setModel(t);
                ui->TableView->resizeColumnsToContents();
                QMessageBox box;
                box.setIcon(QMessageBox::Warning);
                box.setText("Warning: Note that the sum of row "+QString::number(r+1)+
                            " of casting '"+t->readName()+"' is "+QString::number(double(sum))
                            +", which is not equal to 1. Continue check?");
                box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
                box.setDefaultButton(QMessageBox::Yes);
                if(box.exec()!=QMessageBox::Yes) return;
            }
        }
    }

    if(!warnings) ui->statusbar->showMessage("Model checked and found consistent.",5000);
    else ui->statusbar->showMessage("Model checked and found workable (with "+QString::number(warnings)+(warnings>1?" warnings).":" warning)."),5000);

    setChecked(true);

}

// Run the model
void Stox::on_actionRun_triggered()
{
    if(!Saved) {
        QMessageBox box;
        box.setIcon(QMessageBox::Warning);
        box.setText("The model has not been saved in its current state. Save?");
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::Yes);
        if(box.exec()==QMessageBox::Yes) on_actionSave_triggered();
    }

    if(!Checked) {
        QMessageBox box;
        box.setIcon(QMessageBox::Warning);
        box.setText("The model has not been checked in its current state. Check or cancel?");
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Yes);
        if(box.exec()==QMessageBox::Yes) on_actionCheck_triggered();
        if(!Checked) {
            ui->statusbar->showMessage("Cannot run a model not validated by checking.",5000);
            return;
        }
    }

    // Make sure model output screen is visible
    if(ui->tabWidget->currentIndex()==0) ui->tabWidget->setCurrentIndex(1);


    // Read model parameters
    float N=ui->EInitial->text().toFloat();  // Inital population
    int Iters=ui->EIters->text().toInt();    // Iterations tu run
    Eps=ui->EEps->text().toFloat();          // Quasi-zero value of the tail of the probability distribution

    // Count stages to be reported
    int cols=1;
    QTreeWidgetItemIterator iat(ui->TreeWid);
    while(*iat) {
        if((*iat)->checkState(2)==Qt::Checked) cols++;
        ++iat;
    }

    // Set the table for the outputs
    if(cols<5) cols=5;
    if(Output) delete Output;
    Output=new OutTableModel;
    Output->Init(Iters+3,cols);
    ui->TVOutput->setModel(Output);

    // Build the header
    Output->setCell(0,1,"Initial");
    Output->setCell(0,2,QString::number(N));
    Output->setCell(0,3,"Eps");
    Output->setCell(0,4,QString::number(Eps));
    Output->setCell(2,0,"Iter");
    cols=1;
    QTreeWidgetItemIterator it(ui->TreeWid);
    while(*it) {
        if((*it)->checkState(2)==Qt::Checked) {
            Output->setCell(1,cols,(*it)->text(3));
            Output->setCell(2,cols,(*it)->text(0));
            ++cols;
        }
        ++it;
    }
    ui->TVOutput->resizeColumnsToContents();


    // Iterate
    ui->BCancel->show();  // Show the cancel button to allow the user to interrupt a long run
    GoOn=true;
    for(int i=1;i<=Iters&&GoOn;++i) {
        // Output iteration number
        Output->setCell(i+2,0,QString("%1").arg(i,4));

        // Trig the casting at the top of the model tree
        Cast(*ui->TreeWid->topLevelItem(0),N);

        // Output iteration results
        QTreeWidgetItemIterator itt(ui->TreeWid);
        cols=1;
        while(*itt) {
            if((*itt)->checkState(2)==Qt::Checked) {
                Output->setCell(i+2,cols,QString("%1").arg((*itt)->data(0,Qt::UserRole).toFloat(),10,'f',3));
                ++cols;
            }
            ++itt;
        }
        Output->updateRow(i+2);

        // Keep an eye on any user input
        QApplication::processEvents();

    }

    // We are done
    ui->BCancel->hide();

    ui->statusbar->showMessage("Model successfully ran.",5000);

}

// Process stage in model run
void Stox::Cast(QTreeWidgetItem &node, float n) {
    // Receive population
    node.setData(0,Qt::UserRole,n);
    // Terminal stage: all ends here
    if(node.text(1)=="Success"||node.text(1)=="Sink") return;
    // Direct transit stage: pass the whole lot
    if(node.text(1)=="Direct") {
        Cast(*node.child(0),n);
        return;
    }
    // Caster stage: Cast population to following stages
    const QString &Casting=node.text(1);
    for(auto &&t: Tables) {
        // Locate casting table
        if(t->readName()==Casting) {
            int r=t->readRows();
            if(r>1) {
                // Bootstrap
                std::uniform_int_distribution<int> distr(0,r-1);
                r=distr(*generator);
            }
            // Distribute the lot
            for(int c=0;c<node.childCount();++c) {
                float f=t->readCell(r,c);
                Cast(*node.child(c),n*(f>0.0?f:Eps));
            }
            break;
        }
    }
}


// Abort model run
void Stox::on_BCancel_clicked()
{
    GoOn=false;
}


// Copy to the system clipboard the whole contents of the model output table
void Stox::on_BCopyAll_clicked()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    QString text;
    int R=Output->readRows(), C=Output->readCols();
    for(int r=0;r<R;++r) for(int c=0;c<C;++c) text+=Output->readCell(r,c)+(c<C-1?'\t':'\n');
    clipboard->setText(text);
    ui->statusbar->showMessage("Model output copied to clipboard.",5000);
}

// Save to file the contents of the model output table
void Stox::on_BSaveOutput_clicked()
{
    QString filename=QFileDialog::getSaveFileName(this, tr("Save model output"),Path,tr("Text file (*.txt);; HTML file (*.html)"));
    if(filename.isEmpty()) return;

    QFile f(filename);
    if(f.open(QIODevice::WriteOnly|QIODevice::Text)) {
        QTextStream stream(&f);
        QFileInfo fi(filename);
        if(fi.suffix()=="html"||fi.suffix()=="htm") { // html format
            QString text="<html><table><tr><td>";
            int R=Output->readRows(), C=Output->readCols();
            for(int r=0;r<R;++r) for(int c=0;c<C;++c) text+=Output->readCell(r,c)+(c<C-1?"</td><td>":"</td></tr><tr><td>");
            stream<<text;
        } else {            // tab separated text format
            QString text;
            int R=Output->readRows(), C=Output->readCols();
            for(int r=0;r<R;++r) for(int c=0;c<C;++c) text+=Output->readCell(r,c)+(c<C-1?'\t':'\n');
            stream<<text;
        }
        f.close();
        ui->statusbar->showMessage("Model output saved to "+filename,5000);
    } else ui->statusbar->showMessage("ERROR: Couldn't save model output to "+filename,5000);
}


// Select a new file name for the model
void Stox::on_actionSave_as_triggered()
{
    QString filename=QFileDialog::getSaveFileName(this, tr("Save model"),"..",tr("StoX model file (*.sxm)"));
    if(filename.isEmpty()) return;
    FileName=filename;
    QFileInfo finfo(FileName);
    Path=finfo.path();
    setWindowTitle("StoX v3.1 - "+finfo.fileName());
    on_actionSave_triggered();
}

// Save model to file using known name
void Stox::on_actionSave_triggered()
{
    if(FileName.isEmpty()||FileName.isNull()) {on_actionSave_as_triggered(); return;}
    // Serialize the model
    DumpList.clear();
    Dump(*ui->TreeWid->topLevelItem(0),0);
    QFile file(FileName);
    if (file.open(QIODevice::WriteOnly)) {
        QDataStream stream(&file);
        // Save the serialized model
        stream<<(int)DumpList.size();
        for(auto &&i:DumpList) {
            stream<<i.getLevel();
            stream<<*(i.getItem());
        }
        // Save the castings
        int n=Tables.size();
        stream<<n;
        for(auto &&t: Tables) {
            stream<<t->readName();
            int rows=t->readRows(), cols=t->readCols();
            stream<<rows<<cols;
            for(int r=0;r<rows;++r) for(int c=0;c<cols;++c) stream<<t->readCell(r,c);
        }
        file.close();
        setSaved(true);
        ui->statusbar->showMessage("Save model: Model saved to "+FileName,5000);
        return;
    }
    ui->statusbar->showMessage("Save model ERROR: Couldn't save model to "+FileName,5000);
}

void Stox::Dump(QTreeWidgetItem &node, int n) {

    DumpList.emplace_back(NodeData(n,&node));
    for(int c=0;c<node.childCount();++c) Dump(*node.child(c),n+1);

}

// Retrieve model from file
void Stox::on_actionOpen_triggered()
{

    if(!Saved) {
        QMessageBox box;
        box.setIcon(QMessageBox::Warning);
        box.setText("The current model has not been saved in its current state. Save?");
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No  | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Yes);
        int res=box.exec();
        if(res==QMessageBox::Yes) on_actionSave_triggered();
        else if(res==QMessageBox::Cancel) return;
    }

    QString filename=QFileDialog::getOpenFileName(this, tr("Open model"),"..",tr("StoX model file (*.sxm)"));
    if(filename.isEmpty()) return;
    QFileInfo finfo(filename);
    Path=finfo.path();

    ui->TreeWid->clear();
    DumpList.clear();
    QFile file(filename);
    if(file.open(QIODevice::ReadOnly)) {

        QDataStream stream(&file);
        // Read the serialized model
        int stages;
        stream>>stages;
        for(int i=0;i<stages&&!stream.atEnd();++i) {
            int n;
            stream>>n;
            QTreeWidgetItem *item=new QTreeWidgetItem;
            stream>>*item;
            DumpList.emplace_back(NodeData(n,item));
        }
        // Read the castings
        ui->CBCastings->clear();
        for(auto &&t: Tables) delete t;
        Tables.clear();
        stream>>NumTables;
        Tables.resize(NumTables);
        QStringList tablenames;
        for(auto &&t: Tables) {
            QString name;
            stream>>name;
            tablenames<<name;
            t=new TableModel;
            t->setName(name);
            int rows, cols;
            stream>>rows>>cols;
            float *raw=new float[rows*cols];
            for(int i=0;i<rows*cols;++i) stream>>raw[i];
            t->FillFromRaw(raw,rows,cols);
            delete [] raw;
        }

        file.close();
        FileName=filename;
        setSaved(true);
        setChecked(false);
        setWindowTitle("StoX v3.1 - "+finfo.fileName());

        // Rebuild the model tree
        for(int i=0;i<stages;++i) {
            // Deserialize the model tree stage by stage
            NodeData node(DumpList.back().getLevel(),DumpList.back().getItem());
            DumpList.pop_back();
            int pl=node.getLevel()-1; qDebug()<<node.getItem()->text(0);
            if(auto par=std::find_if(rbegin(DumpList),rend(DumpList),[pl](NodeData nod) {return nod.getLevel()==pl;}); par!=std::rend(DumpList)) {
                (*par).getItem()->insertChild(0,node.getItem());
            } else {
                ui->TreeWid->addTopLevelItem(node.getItem());
                break;
            }
        }

        // Fill the list of castings and sort by name
        ui->CBCastings->addItems(tablenames);
        ui->CBCastings->model()->sort(0);

        // Expand the full model tree
        on_BExpandTree_clicked();

        if(ui->tabWidget->currentIndex()==1) ui->tabWidget->setCurrentIndex(0);

        ui->statusbar->showMessage("Open model: Model "+FileName+" successfully opened.",5000);     

    } else {
        ui->statusbar->showMessage("Open model: Could not open model "+filename,5000);
        on_actionNew_triggered();
    }

}


// New model
void Stox::on_actionNew_triggered()
{
    if(!Saved) {
        QMessageBox box;
        box.setIcon(QMessageBox::Warning);
        box.setText("The current model has not been saved in its current state. Save?");
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::Yes);
        if(box.exec()==QMessageBox::Yes) on_actionSave_triggered();
    }

    FileName.clear();
    setWindowTitle("StoX v3.1 - Not saved");

    // Empty and set up the model tree
    ui->TreeWid->clear();
    ui->TreeWid->addTopLevelItem(new QTreeWidgetItem(static_cast<QTreeWidget *>(nullptr), QStringList() << "Start"));

    // Empty castings list
    if(!Tables.empty()) for(auto &&t: Tables) delete t;
    Tables.clear();
    NumTables=0;
    ui->CBCastings->clear();

    if(ui->tabWidget->currentIndex()==1) ui->tabWidget->setCurrentIndex(0);

    setSaved(false);
    FileName.clear();
    setChecked(false);

}


// Quit
void Stox::on_actionExit_triggered()
{

    QMessageBox box;
    box.setText("Do you want to quit?");
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Yes);
    if(box.exec()==QMessageBox::Yes) {

        if(!Saved) {
            QMessageBox box;
            box.setIcon(QMessageBox::Warning);
            box.setText("The model has not been saved in its current state. Save?");
            box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            box.setDefaultButton(QMessageBox::Yes);
            if(box.exec()==QMessageBox::Yes) on_actionSave_triggered();
        }

        QFile file("Stox.ini");
        if (file.open(QIODevice::WriteOnly)) {
            QDataStream stream(&file);
            stream<<Path;
            stream<<ui->EIters->text();
            stream<<ui->EInitial->text();
            stream<<ui->EEps->text();
            file.close();
        }

        QCoreApplication::quit();

    }

}

// Show 'about box' with licence information as per GPL guidelines
void Stox::on_actionAbout_triggered()
{

    QMessageBox box;
    box.setIconPixmap(QPixmap(":/images/Resources/CamarinaS.png"));
    box.setText(        "<p><center><strong>StoX</strong>: Stochastic multistage recruitment model</center>"
                        "<p><center>Copyright 2008-2024 J.Martín-Herrero (Universiy of Vigo, Spain)</center></p>"
                        "<p>Concept:</p>"
                        "<font><p style='font-family: Times New Roman'>M.Calviño-Cancela and J.Martín-Herrero (2009) "
                        "\"Effectiveness of a varied assemblage of seed dispersers of a fleshy-fruited plant\" <i>Ecology</i> 90(12):3503-3515.</p>"
                        "<p>This program:</p>"
                        "<font><p style='font-family: Times New Roman'>J.Martín-Herrero and M.Calviño-Cancela (2024) \"StoX: Stochastic multistage recruitment model for seed dispersal effectiveness\" <i>submitted to Software Impacts</i></p>"
                        "<p><center>Licenced under GNU GPLv3</center></p>"
                        "This program is free software; you can redistribute it and/or modify it under "
                        "the terms of the GNU General Public License as published by the Free Software "
                        "Foundation; either version 3 of the License, or at your option any later version. "
                        "This program is distributed in the hope that it will be useful, but WITHOUT "
                        "ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS "
                        "FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. "
                        "You should have received a copy of the GNU General Public License along with "
                        "this program. If not, see <tt>https://www.gnu.org/licenses/</tt>");
    box.exec();
}



//---------------------------------------------------------------------------------------








