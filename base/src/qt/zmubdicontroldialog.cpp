/***********************************************************************
*************Copyright (c) 2015-2018 The PIVX developers****************
******************Copyright (c) 2010-2019 Nur1Labs**********************
>Distributed under the MIT/X11 software license, see the accompanying
>file COPYING or http://www.opensource.org/licenses/mit-license.php.
************************************************************************/

#include "zmubdicontroldialog.h"
#include "ui_zmubdicontroldialog.h"

#include "accumulators.h"
#include "main.h"
#include "walletmodel.h"

using namespace std;

std::set<std::string> ZMuBdIControlDialog::setSelectedMints;
std::set<CMintMeta> ZMuBdIControlDialog::setMints;

ZMuBdIControlDialog::ZMuBdIControlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ZMuBdIControlDialog),
    model(0)
{
    ui->setupUi(this);
    setMints.clear();
    privacyDialog = (PrivacyDialog*)parent;

    // click on checkbox
    connect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(updateSelection(QTreeWidgetItem*, int)));

    // push select/deselect all button
    connect(ui->pushButtonAll, SIGNAL(clicked()), this, SLOT(ButtonAllClicked()));
}

ZMuBdIControlDialog::~ZMuBdIControlDialog()
{
    delete ui;
}

void ZMuBdIControlDialog::setModel(WalletModel *model)
{
    this->model = model;
    updateList();
}

//Update the tree widget
void ZMuBdIControlDialog::updateList()
{
    // need to prevent the slot from being called each time something is changed
    ui->treeWidget->blockSignals(true);
    ui->treeWidget->clear();

    // add a top level item for each denomination
    QFlags<Qt::ItemFlag> flgTristate = Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
    map<libzerocoin::CoinDenomination, int> mapDenomPosition;
    for (auto denom : libzerocoin::zerocoinDenomList) {
        QTreeWidgetItem* itemDenom(new QTreeWidgetItem);
        ui->treeWidget->addTopLevelItem(itemDenom);

        //keep track of where this is positioned in tree widget
        mapDenomPosition[denom] = ui->treeWidget->indexOfTopLevelItem(itemDenom);

        itemDenom->setFlags(flgTristate);
        itemDenom->setText(COLUMN_DENOMINATION, QString::number(denom));
    }

    // select all unused coins - including not mature. Update status of coins too.
    std::set<CMintMeta> set;
    model->listZerocoinMints(set, true, false, true);
    this->setMints = set;

    //populate rows with mint info
    int nBestHeight = chainActive.Height();
    //map<CoinDenomination, int> mapMaturityHeight = GetMintMaturityHeight(); /** Dark Protocol **/
    for (const CMintMeta& mint : setMints) {
        // assign this mint to the correct denomination in the tree view
        libzerocoin::CoinDenomination denom = mint.denom;
        QTreeWidgetItem *itemMint = new QTreeWidgetItem(ui->treeWidget->topLevelItem(mapDenomPosition.at(denom)));

        // if the mint is already selected, then it needs to have the checkbox checked
        std::string strPubCoinHash = mint.hashPubcoin.GetHex();
        
		if (setSelectedMints.count(strPubCoinHash))
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
		else
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

        itemMint->setText(COLUMN_DENOMINATION, QString::number(mint.denom));
        itemMint->setText(COLUMN_PUBCOIN, QString::fromStdString(strPubCoinHash));
        itemMint->setText(COLUMN_VERSION, QString::number(mint.nVersion));

        int nConfirmations = (mint.nHeight ? nBestHeight - mint.nHeight : 0);
        if (nConfirmations < 0) {
            // Sanity check
            nConfirmations = 0;
        }

        itemMint->setText(COLUMN_CONFIRMATIONS, QString::number(nConfirmations));

        // check for maturity
        bool isMature = false;
        /*if (mapMaturityHeight.count(mint.denom))
            isMature = mint.nHeight < mapMaturityHeight.at(denom);*/ /* Dark Protocol */

        // disable selecting this mint if it is not spendable - also display a reason why
        bool fSpendable = isMature && nConfirmations >= Params().Zerocoin_MintRequiredConfirmations();
        if(!fSpendable) {
            itemMint->setDisabled(true);
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

            //if this mint is in the selection list, then remove it
            if (setSelectedMints.count(strPubCoinHash))
                setSelectedMints.erase(strPubCoinHash);

            string strReason = "";
            if(nConfirmations < Params().Zerocoin_MintRequiredConfirmations())
                strReason = strprintf("Needs %d more confirmations", Params().Zerocoin_MintRequiredConfirmations() - nConfirmations);
            else
                strReason = strprintf("Needs %d more mints added to network", Params().Zerocoin_RequiredAccumulation());

            itemMint->setText(COLUMN_ISSPENDABLE, QString::fromStdString(strReason));
        } else {
            itemMint->setText(COLUMN_ISSPENDABLE, QString("Yes"));
        }
    }

    ui->treeWidget->blockSignals(false);
    updateLabels();
}

// Update the list when a checkbox is clicked
void ZMuBdIControlDialog::updateSelection(QTreeWidgetItem* item, int column)
{
    // only want updates from non top level items that are available to spend
    if (item->parent() && column == COLUMN_CHECKBOX && !item->isDisabled()){

        // see if this mint is already selected in the selection list
        std::string strPubcoin = item->text(COLUMN_PUBCOIN).toStdString();
        bool fSelected = setSelectedMints.count(strPubcoin);

        // set the checkbox to the proper state and add or remove the mint from the selection list
        if (item->checkState(COLUMN_CHECKBOX) == Qt::Checked) {
            if (fSelected) return;
            setSelectedMints.insert(strPubcoin);
        } else {
            if (!fSelected) return;
            setSelectedMints.erase(strPubcoin);
        }
        updateLabels();
    }
}

// Update the Quantity and Amount display
void ZMuBdIControlDialog::updateLabels()
{
    int64_t nAmount = 0;
    for (const CMintMeta& mint : setMints) {
        if (setSelectedMints.count(mint.hashPubcoin.GetHex()))
            nAmount += mint.denom;
    }

    //update this dialog's labels
    ui->labelZMuBdI_int->setText(QString::number(nAmount));
    ui->labelQuantity_int->setText(QString::number(setSelectedMints.size()));

    //update PrivacyDialog labels
    privacyDialog->setZMuBdIControlLabels(nAmount, setSelectedMints.size());
}

std::vector<CMintMeta> ZMuBdIControlDialog::GetSelectedMints()
{
    std::vector<CMintMeta> listReturn;
    for (const CMintMeta& mint : setMints) {
        if (setSelectedMints.count(mint.hashPubcoin.GetHex()))
            listReturn.emplace_back(mint);
    }

    return listReturn;
}

// select or deselect all of the mints
void ZMuBdIControlDialog::ButtonAllClicked()
{
    ui->treeWidget->blockSignals(true);
    Qt::CheckState state = Qt::Checked;
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        if(ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != Qt::Unchecked) {
            state = Qt::Unchecked;
            break;
        }
    }

    //much quicker to start from scratch than to have QT go through all the objects and update
    ui->treeWidget->clear();

    if (state == Qt::Checked) {
        for(const CMintMeta& mint : setMints)
            setSelectedMints.insert(mint.hashPubcoin.GetHex());
    } else {
        setSelectedMints.clear();
    }

    updateList();
}

