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

#ifndef STOX_H
#define STOX_H

#include <QMainWindow>

#include <QAbstractTableModel>
#include <QTreeWidget>
#include <QLabel>
#include <QCloseEvent>
#include <random>

//#include <qDebug>

QT_BEGIN_NAMESPACE
namespace Ui { class Stox; }
QT_END_NAMESPACE


// Casting tables (stage transition probabilities)
class TableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // Always created empty
    explicit TableModel(QObject *parent = 0): QAbstractTableModel(parent) {
        myrows=0;
        mycols=0;
        name="";
    };

    // Fill table from a float array [rowsxcols]
    void FillFromRaw(float *raw, int rows, int cols) {
        beginResetModel();

        myrows=rows;
        mycols=cols;
        rawdata.clear();
        rawdata.resize(rows,std::vector<float>(cols, 0));
        for(auto&& r:rawdata) for(int c=0;c<cols;++c) r[c]=*raw++;

        endResetModel();
    };

    // Fill with same data as another table
    void FillFromCopy(TableModel *source, QString nam) {
        beginResetModel();

        name=nam;
        myrows=source->readRows();
        mycols=source->readCols();
        rawdata.clear();
        rawdata.resize(myrows,std::vector<float>(mycols, 0));
        for(int r=0;r<myrows;++r) for(int c=0;c<mycols;++c) rawdata[r][c]=source->readCell(r,c);

        endResetModel();
    };

    // Fill with zeroes
    void FillZeroes(int rows, int cols, QString nam) {
        beginResetModel();

        name=nam;
        myrows=rows;
        mycols=cols;
        rawdata.clear();
        rawdata.resize(rows,std::vector<float>(cols, 0));
        //for(auto&& r:rawdata) for(int c=0;c<cols;++c) r[c]=0;

        endResetModel();
    };

    // Read number of columns for Table View widget
    int rowCount(const QModelIndex &parent) const override {
        return parent.isValid()?0:myrows;
    };

    // Read number of rows for Table View widget
    int columnCount(const QModelIndex &parent) const override {
        return parent.isValid()?0:mycols;
    };

    // Read number of columns simple
    int readCols() const {
        return mycols;
    };

    // Read number of rows simple
    int readRows() const {
        return myrows;
    };

    // Read sum of columns in a row
    float sumCols(int irow) {
        std::vector<float>&row=rawdata[irow];
        return std::accumulate(row.begin(),row.end(),0.0f);
    }

    // Read casting value in (row r,col c)
    float readCell(int r, int c) {
        return rawdata.at(r).at(c);
    }

    // Set table value at (row r,col c)
    void setCell(int r, int c, float v) {
        rawdata.at(r).at(c)=v;
    }

    // Read table data for Table View widget
    QVariant data(const QModelIndex &index, int role) const override {
        if(!index.isValid()) return QVariant();
        if(index.row()>=myrows || index.row()<0) return QVariant();
        if(role==Qt::TextAlignmentRole) return int(Qt::AlignRight | Qt::AlignVCenter);
        if(role==Qt::DisplayRole) return rawdata.at(index.row()).at(index.column());
        return QVariant();
    };

    // Edit casting value with Table View widget
    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if(index.isValid() && role==Qt::EditRole) {
            float val=value.toFloat();
            // Prevent negative values and values greater than 1.0
            if(val<0.0f) val=0.0f;
            else if(val>1.0f) val=1.0f;
            // Prevent row sum grater than 1.0
            std::vector<float>&row=rawdata[index.row()];
            float &pos=row[index.column()];
            float sum=std::accumulate(row.begin(),row.end(),-pos);
            if(sum+val>1.0f) val=1.0f-sum;
            pos=val;
            emit dataChanged(index, index, {role});
            return true;
        }
        return false;
    };

    // To allow editing value from Table Wiew widget
    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if(!index.isValid()) return Qt::ItemIsEnabled;
        return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
    };

    // Set casting name
    void setName(QString nam)
    {
        name=nam;
    }

    // Read casting name
    QString readName()
    {
        return name;
    }

private:
    // Casting values in a std vector of vectors of floats
    std::vector<std::vector<float>> rawdata;
    // Rows and columns
    int myrows, mycols;
    // Casting name
    QString name;

};

// Validator class for casting names editor
class NameValidator : public QValidator
{
    Q_OBJECT
public:
    explicit NameValidator(QObject *parent = 0): QValidator(parent){}
    virtual State validate ( QString & input, int & pos ) const
    {
        // Prevent reserved names to be used as casting names
        if(input=="Sink" || input=="Success" || input=="Direct") return Invalid;
        return Acceptable;
    }
};


// Output table
class OutTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // Always created empty
    explicit OutTableModel(QObject *parent = 0): QAbstractTableModel(parent) {
        myrows=0;
        mycols=0;
    }

    // Init table with given size
    void Init(int rows, int cols) {
        beginResetModel();

        myrows=rows;
        mycols=cols;
        rawdata.clear();
        rawdata.resize(rows,std::vector<QString>(cols,""));

        endResetModel();
    }

    // Read number of columns for Table View widget
    int rowCount(const QModelIndex &parent) const override {return parent.isValid()?0:myrows;}

    // Read number of rows for Table View widget
    int columnCount(const QModelIndex &parent) const override {return parent.isValid()?0:mycols;}

    // Read number of columns simple
    int readCols() const {return mycols;}

    // Read number of rows simple
    int readRows() const {return myrows;}

    // Read value in (row r,col c)
    QString readCell(int r, int c) {return rawdata.at(r).at(c);}

    // Set table value at (row r,col c)
    void setCell(int r, int c, QString v) {rawdata.at(r).at(c)=v;}

    void updateRow(int r) {emit dataChanged(this->index(r,0),this->index(r,mycols-1), {Qt::DisplayRole});}

    // Read table data for Table View widget
    QVariant data(const QModelIndex &index, int role) const override {
        if(!index.isValid()) return QVariant();
        if(index.row()>=myrows || index.row()<0) return QVariant();
        if(role==Qt::TextAlignmentRole) return index.row()<3?int(Qt::AlignCenter | Qt::AlignVCenter):int(Qt::AlignRight | Qt::AlignVCenter);
        if(role==Qt::DisplayRole) return rawdata.at(index.row()).at(index.column());
        return QVariant();
    }

    // Edit value with Table View widget
    bool setData(const QModelIndex &index, const QVariant &value, int role) override
    {
        if(index.isValid() && role==Qt::EditRole) {
            std::vector<QString>&row=rawdata[index.row()];
            QString &pos=row[index.column()];
            pos=value.toString();
            emit dataChanged(index, index, {role});
            return true;
        }
        return false;
    }

    // To allow editing value from Table Wiew widget
    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if(!index.isValid()) return Qt::ItemIsEnabled;
        return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
    }


private:
    // Table values in a std vector of vectors of QStrings
    std::vector<std::vector<QString>> rawdata;
    // Rows and columns
    int myrows, mycols;

};



// Tree node data for temporary storage during save & open operations
class NodeData {
public:
    NodeData(int lev, QTreeWidgetItem *it): level(lev),item(it) {}
    ~NodeData() {}
    int getLevel() {return level;}
    QTreeWidgetItem* getItem() {return item;}
private:
    // Level is required to put the node back in its place in the tree
    int level;
    QTreeWidgetItem *item;
};

// The main window class
class Stox : public QMainWindow
{
    Q_OBJECT

public:
    Stox(QWidget *parent = nullptr);
    ~Stox();

    void closeEvent(QCloseEvent *event) {
        on_actionExit_triggered();
        event->accept();
    }

private slots:
    // Slots to respond to user interaction: control names are self-explaining
    void on_BNewTable_clicked();

    void on_BRenameTable_clicked();

    void on_CBCastings_currentIndexChanged(int index);

    void on_BDelTable_clicked();

    void on_BChild_clicked();

    void on_BSibling_clicked();

    void on_BRenameStage_clicked();

    void on_BSetCasting_clicked();

    void on_TreeWid_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    void on_BRemoveNode_clicked();

    void on_BCloneNode_clicked();

    void on_BShowAll_clicked();

    void on_BShowNone_clicked();

    void on_RBNodeTypeDirect_toggled(bool checked);

    void on_RBNodeTypeCaster_toggled(bool checked);

    void on_RBNodeTypeSuccess_toggled(bool checked);

    void on_RBNodeTypeSink_toggled(bool checked);

    void on_BSetType_clicked();

    void on_actionCheck_triggered();

    void on_actionRun_triggered();

    void on_BCancel_clicked();

    void on_BCopyAll_clicked();

    void on_BSaveOutput_clicked();

    void on_BDupliTable_clicked();

    void on_BPasteTable_clicked();

    void on_BExpandTree_clicked();

    void on_actionSave_triggered();

    void on_actionSave_as_triggered();

    void on_actionOpen_triggered();

    void on_actionNew_triggered();

    void on_actionExit_triggered();

    void on_actionAbout_triggered();

    void on_BShowSuccess_clicked();

private:
    Ui::Stox *ui;

    QLabel *LSave;
    QLabel *LCheck;

    std::list<TableModel*> Tables; // The list of castings
    int NumTables;

    OutTableModel *Output;

    bool Checked;   // Status flag: model has been checked since last modification
    bool Saved;     // Status flag: model has been saved since last modification
    QString FileName;   // File name of saved model
    QString Path;       // Path to the folder where model has been last saved or opened

    std::random_device  rand_dev;   // Random generator for bootstrapping
    std::mt19937 *generator;

    float Eps;      // The quasi-zero value of the distribution tail

    bool GoOn;      // Flag to abort model run

    int NodeType;   // Type of stage: Direct, Caster, Sink, or Success.
    QStringList TypeNames;

    bool CloneMode; // Flag: Program is expecting a click on the model tree to replicate another stage
    QTreeWidgetItem *SourceClone;   // Stage to be replicated on another spot in the model tree

    std::list<NodeData> DumpList;   // Serialized model tree for storage purposes

    void setChecked(bool stat);
    void setSaved(bool stat);

    // Assigns a unique hyerachical ID to each stage in the model tree for identification
    void IDMarkTree();
    // Add a stage to the model tree
    void AddTreeChild(QTreeWidgetItem *parent, QString name, QString cast, bool show);
    // Expand the model tree
    void Xpand(QTreeWidgetItem &item);
    // Apply bootstrapped transistion probabilities to stage node with a population n
    void Cast(QTreeWidgetItem &node, float n);
    // Store stage node into serialized list
    void Dump(QTreeWidgetItem &node, int n);
    // Remove stage node
    void RemoveNode(QTreeWidgetItem *node);

};
#endif // STOX_H
