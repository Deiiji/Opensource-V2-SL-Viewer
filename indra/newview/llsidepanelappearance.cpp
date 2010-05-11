/**
 * @file llsidepanelappearance.cpp
 * @brief Side Bar "Appearance" panel
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009-2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 * 
 */

#include "llviewerprecompiledheaders.h"
#include "llsidepanelappearance.h"

#include "llaccordionctrltab.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llinventorypanel.h"
#include "llfiltereditor.h"
#include "llfloaterreg.h"
#include "llfloaterworldmap.h"
#include "llfoldervieweventlistener.h"
#include "llpaneleditwearable.h"
#include "llpaneloutfitsinventory.h"
#include "llsidetray.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llwearable.h"

static LLRegisterPanelClassWrapper<LLSidepanelAppearance> t_appearance("sidepanel_appearance");

class LLCurrentlyWornFetchObserver : public LLInventoryFetchItemsObserver
{
public:
	LLCurrentlyWornFetchObserver(const uuid_vec_t &ids,
								 LLSidepanelAppearance *panel) :
		LLInventoryFetchItemsObserver(ids),
		mPanel(panel)
	{}
	~LLCurrentlyWornFetchObserver() {}
	virtual void done()
	{
		mPanel->inventoryFetched();
		gInventory.removeObserver(this);
		delete this;
	}
private:
	LLSidepanelAppearance *mPanel;
};

class LLWatchForOutfitRenameObserver : public LLInventoryObserver
{
public:
	LLWatchForOutfitRenameObserver(LLSidepanelAppearance *panel) :
		mPanel(panel)
	{}
	virtual void changed(U32 mask);
	
private:
	LLSidepanelAppearance *mPanel;
};

void LLWatchForOutfitRenameObserver::changed(U32 mask)
{
	if (mask & LABEL)
	{
		mPanel->refreshCurrentOutfitName();
	}
}

LLSidepanelAppearance::LLSidepanelAppearance() :
	LLPanel(),
	mFilterSubString(LLStringUtil::null),
	mFilterEditor(NULL),
	mOutfitEdit(NULL),
	mCurrOutfitPanel(NULL)
{
}

LLSidepanelAppearance::~LLSidepanelAppearance()
{
	gInventory.removeObserver(mOutfitRenameWatcher);
	delete mOutfitRenameWatcher;
}

// virtual
BOOL LLSidepanelAppearance::postBuild()
{
	mOpenOutfitBtn = getChild<LLButton>("openoutfit_btn");
	mOpenOutfitBtn->setClickedCallback(boost::bind(&LLSidepanelAppearance::onOpenOutfitButtonClicked, this));

	mEditAppearanceBtn = getChild<LLButton>("editappearance_btn");
	mEditAppearanceBtn->setClickedCallback(boost::bind(&LLSidepanelAppearance::onEditAppearanceButtonClicked, this));

	childSetAction("edit_outfit_btn", boost::bind(&LLSidepanelAppearance::onEditOutfitButtonClicked, this));

	mEditBtn = getChild<LLButton>("edit_btn");
	mEditBtn->setClickedCallback(boost::bind(&LLSidepanelAppearance::onEditButtonClicked, this));

	mNewOutfitBtn = getChild<LLButton>("newlook_btn");
	mNewOutfitBtn->setClickedCallback(boost::bind(&LLSidepanelAppearance::onNewOutfitButtonClicked, this));
	mNewOutfitBtn->setEnabled(false);

	mFilterEditor = getChild<LLFilterEditor>("Filter");
	if (mFilterEditor)
	{
		mFilterEditor->setCommitCallback(boost::bind(&LLSidepanelAppearance::onFilterEdit, this, _2));
	}

	mPanelOutfitsInventory = dynamic_cast<LLPanelOutfitsInventory *>(getChild<LLPanel>("panel_outfits_inventory"));
	mPanelOutfitsInventory->setParent(this);

	mOutfitEdit = dynamic_cast<LLPanelOutfitEdit*>(getChild<LLPanel>("panel_outfit_edit"));
	if (mOutfitEdit)
	{
		LLButton* back_btn = mOutfitEdit->getChild<LLButton>("back_btn");
		if (back_btn)
		{
			back_btn->setClickedCallback(boost::bind(&LLSidepanelAppearance::showOutfitsInventoryPanel, this));
		}

	}

	mEditWearable = dynamic_cast<LLPanelEditWearable*>(getChild<LLPanel>("panel_edit_wearable"));
	if (mEditWearable)
	{
		LLButton* edit_wearable_back_btn = mEditWearable->getChild<LLButton>("back_btn");
		if (edit_wearable_back_btn)
		{
			edit_wearable_back_btn->setClickedCallback(boost::bind(&LLSidepanelAppearance::onEditWearBackClicked, this));
		}
	}

	mCurrentLookName = getChild<LLTextBox>("currentlook_name");

	mOutfitStatus = getChild<LLTextBox>("currentlook_status");
	
	mCurrOutfitPanel = getChild<LLPanel>("panel_currentlook");

	mOutfitRenameWatcher = new LLWatchForOutfitRenameObserver(this);
	gInventory.addObserver(mOutfitRenameWatcher);

	return TRUE;
}

// virtual
void LLSidepanelAppearance::onOpen(const LLSD& key)
{
	fetchInventory();
	refreshCurrentOutfitName();
	updateVerbs();

	if (mPanelOutfitsInventory)
	{
		mPanelOutfitsInventory->onOpen(key);
	}

	if(key.size() == 0)
		return;
	
	toggleOutfitEditPanel(TRUE);
	updateVerbs();
	
	mLookInfoType = key["type"].asString();

	if (mLookInfoType == "edit_outfit")
	{
		mOutfitEdit->displayCurrentOutfit();
	}
}

void LLSidepanelAppearance::onFilterEdit(const std::string& search_string)
{
	if (mFilterSubString != search_string)
	{
		mFilterSubString = search_string;

		// Searches are case-insensitive
		// but we don't convert the typed string to upper-case so that it can be fed to the web search as-is.

		mPanelOutfitsInventory->onSearchEdit(mFilterSubString);
	}
}

void LLSidepanelAppearance::onOpenOutfitButtonClicked()
{
	const LLViewerInventoryItem *outfit_link = LLAppearanceMgr::getInstance()->getBaseOutfitLink();
	if (!outfit_link)
		return;
	if (!outfit_link->getIsLinkType())
		return;

	LLAccordionCtrlTab* tab_outfits = mPanelOutfitsInventory->findChild<LLAccordionCtrlTab>("tab_outfits");
	if (tab_outfits)
	{
		tab_outfits->changeOpenClose(FALSE);
		LLInventoryPanel *inventory_panel = tab_outfits->findChild<LLInventoryPanel>("outfitslist_tab");
		if (inventory_panel)
		{
			LLFolderView* root = inventory_panel->getRootFolder();
			LLFolderViewItem *outfit_folder = root->getItemByID(outfit_link->getLinkedUUID());
			if (outfit_folder)
			{
				outfit_folder->setOpen(!outfit_folder->isOpen());
				root->setSelectionFromRoot(outfit_folder,TRUE);
				root->scrollToShowSelection();
			}
		}
	}
}

void LLSidepanelAppearance::onEditAppearanceButtonClicked()
{
	if (gAgentWearables.areWearablesLoaded())
	{
		gAgentCamera.changeCameraToCustomizeAvatar();
	}
}

void LLSidepanelAppearance::onEditOutfitButtonClicked()
{
	LLSD key;
	key["type"] = "edit_outfit";
	LLSideTray::getInstance()->showPanel("sidepanel_appearance", key);
}

void LLSidepanelAppearance::onEditButtonClicked()
{
	toggleOutfitEditPanel(FALSE);
	toggleWearableEditPanel(TRUE, NULL);
	/*if (mOutfitEdit->getVisible())
	  {
	  }
	  else
	  {
	  mPanelOutfitsInventory->onEdit();
	  }*/
}

void LLSidepanelAppearance::onNewOutfitButtonClicked()
{
	if (!mOutfitEdit->getVisible())
	{
		mPanelOutfitsInventory->onSave();
	}
}

void LLSidepanelAppearance::onEditWearBackClicked()
{
	mEditWearable->saveChanges();
	toggleWearableEditPanel(FALSE, NULL);
	toggleOutfitEditPanel(TRUE);
}

void LLSidepanelAppearance::showOutfitsInventoryPanel()
{
	mOutfitEdit->setVisible(FALSE);

	mPanelOutfitsInventory->setVisible(TRUE);

	mFilterEditor->setVisible(TRUE);
	mEditBtn->setVisible(TRUE);
	mNewOutfitBtn->setVisible(TRUE);
	mCurrOutfitPanel->setVisible(TRUE);
}

void LLSidepanelAppearance::showOutfitEditPanel()
{
	mOutfitEdit->setVisible(TRUE);
	
	mPanelOutfitsInventory->setVisible(FALSE);

	mFilterEditor->setVisible(FALSE);
	mEditBtn->setVisible(FALSE);
	mNewOutfitBtn->setVisible(FALSE);
	mCurrOutfitPanel->setVisible(FALSE);
}

void LLSidepanelAppearance::toggleOutfitEditPanel(BOOL visible)
{
	if (!mOutfitEdit)
		return;

	mOutfitEdit->setVisible(visible);
	if (mPanelOutfitsInventory) mPanelOutfitsInventory->setVisible(!visible);
	mFilterEditor->setVisible(!visible);
	mEditBtn->setVisible(!visible);
	mNewOutfitBtn->setVisible(!visible);
	mCurrOutfitPanel->setVisible(!visible);
}

void LLSidepanelAppearance::toggleWearableEditPanel(BOOL visible, LLWearable *wearable)
{
	if (!wearable)
	{
		wearable = gAgentWearables.getWearable(WT_SHAPE, 0);
	}
	if (!mEditWearable || !wearable)
	{
		return;
	}

	mCurrOutfitPanel->setVisible(!visible);

	mEditWearable->setVisible(visible);
	mEditWearable->setWearable(wearable);
	mFilterEditor->setVisible(!visible);
	mPanelOutfitsInventory->setVisible(!visible);
}

void LLSidepanelAppearance::updateVerbs()
{
	bool is_look_info_visible = mOutfitEdit->getVisible();

	if (mPanelOutfitsInventory && !is_look_info_visible)
	{
//		const bool is_correct_type = (mPanelOutfitsInventory->getCorrectListenerForAction() != NULL);
//		mEditBtn->setEnabled(is_correct_type);
	}
	else
	{
		mEditBtn->setEnabled(FALSE);
	}
}

void LLSidepanelAppearance::refreshCurrentOutfitName(const std::string& name)
{
	// Set current outfit status (wearing/unsaved).
	bool dirty = LLAppearanceMgr::getInstance()->isOutfitDirty();
	std::string cof_status_str = getString(dirty ? "Unsaved Changes" : "Now Wearing");
	mOutfitStatus->setText(cof_status_str);

	if (name == "")
	{
		std::string outfit_name;
		if (LLAppearanceMgr::getInstance()->getBaseOutfitName(outfit_name))
		{
				mCurrentLookName->setText(outfit_name);
				return;
		}
		mCurrentLookName->setText(getString("No Outfit"));
		mOpenOutfitBtn->setEnabled(FALSE);
	}
	else
	{
		mCurrentLookName->setText(name);
		// Can't just call update verbs since the folder link may not have been created yet.
		mOpenOutfitBtn->setEnabled(TRUE);
	}
}

//static
void LLSidepanelAppearance::editWearable(LLWearable *wearable, void *data)
{
	LLSidepanelAppearance *panel = (LLSidepanelAppearance*) data;
	panel->toggleOutfitEditPanel(FALSE);
	panel->toggleWearableEditPanel(TRUE, wearable);
}

// Fetch currently worn items and only enable the New Look button after everything's been
// fetched.  Alternatively, we could stuff this logic into llagentwearables::makeNewOutfitLinks.
void LLSidepanelAppearance::fetchInventory()
{

	mNewOutfitBtn->setEnabled(false);
	uuid_vec_t ids;
	LLUUID item_id;
	for(S32 type = (S32)WT_SHAPE; type < (S32)WT_COUNT; ++type)
	{
		for (U32 index = 0; index < gAgentWearables.getWearableCount((EWearableType)type); ++index)
		{
			item_id = gAgentWearables.getWearableItemID((EWearableType)type, index);
			if(item_id.notNull())
			{
				ids.push_back(item_id);
			}
		}
	}

	if (isAgentAvatarValid())
	{
		for (LLVOAvatar::attachment_map_t::const_iterator iter = gAgentAvatarp->mAttachmentPoints.begin(); 
			 iter != gAgentAvatarp->mAttachmentPoints.end(); ++iter)
		{
			LLViewerJointAttachment* attachment = iter->second;
			if (!attachment) continue;
			for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
				 attachment_iter != attachment->mAttachedObjects.end();
				 ++attachment_iter)
			{
				LLViewerObject* attached_object = (*attachment_iter);
				if (!attached_object) continue;
				const LLUUID& item_id = attached_object->getItemID();
				if (item_id.isNull()) continue;
				ids.push_back(item_id);
			}
		}
	}

	LLCurrentlyWornFetchObserver *fetch_worn = new LLCurrentlyWornFetchObserver(ids, this);
	fetch_worn->startFetch();
	// If no items to be fetched, done will never be triggered.
	// TODO: Change LLInventoryFetchItemsObserver::fetchItems to trigger done() on this condition.
	if (fetch_worn->isFinished())
	{
		fetch_worn->done();
	}
	else
	{
		gInventory.addObserver(fetch_worn);
	}
}

void LLSidepanelAppearance::inventoryFetched()
{
	mNewOutfitBtn->setEnabled(true);
}
