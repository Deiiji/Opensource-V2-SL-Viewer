/** 
 * @file llappearancemgr.cpp
 * @brief Manager for initiating appearance changes on the viewer
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2010, Linden Research, Inc.
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

#include "llaccordionctrltab.h"
#include "llagent.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llcommandhandler.h"
#include "llfloatercustomize.h"
#include "llgesturemgr.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventoryobserver.h"
#include "llnotificationsutil.h"
#include "llpaneloutfitsinventory.h"
#include "llselectmgr.h"
#include "llsidepanelappearance.h"
#include "llsidetray.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llviewerregion.h"
#include "llwearablelist.h"

char ORDER_NUMBER_SEPARATOR('@');

LLUUID findDescendentCategoryIDByName(const LLUUID& parent_id, const std::string& name)
{
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	LLNameCategoryCollector has_name(name);
	gInventory.collectDescendentsIf(parent_id,
									cat_array,
									item_array,
									LLInventoryModel::EXCLUDE_TRASH,
									has_name);
	if (0 == cat_array.count())
		return LLUUID();
	else
	{
		LLViewerInventoryCategory *cat = cat_array.get(0);
		if (cat)
			return cat->getUUID();
		else
		{
			llwarns << "null cat" << llendl;
			return LLUUID();
		}
	}
}

class LLWearInventoryCategoryCallback : public LLInventoryCallback
{
public:
	LLWearInventoryCategoryCallback(const LLUUID& cat_id, bool append)
	{
		mCatID = cat_id;
		mAppend = append;
	}
	void fire(const LLUUID& item_id)
	{
		/*
		 * Do nothing.  We only care about the destructor
		 *
		 * The reason for this is that this callback is used in a hack where the
		 * same callback is given to dozens of items, and the destructor is called
		 * after the last item has fired the event and dereferenced it -- if all
		 * the events actually fire!
		 */
	}

protected:
	~LLWearInventoryCategoryCallback()
	{
		llinfos << "done all inventory callbacks" << llendl;
		
		// Is the destructor called by ordinary dereference, or because the app's shutting down?
		// If the inventory callback manager goes away, we're shutting down, no longer want the callback.
		if( LLInventoryCallbackManager::is_instantiated() )
		{
			LLAppearanceMgr::instance().wearInventoryCategoryOnAvatar(gInventory.getCategory(mCatID), mAppend);
		}
		else
		{
			llwarns << "Dropping unhandled LLWearInventoryCategoryCallback" << llendl;
		}
	}

private:
	LLUUID mCatID;
	bool mAppend;
};


//Inventory callback updating "dirty" state when destroyed
class LLUpdateDirtyState: public LLInventoryCallback
{
public:
	LLUpdateDirtyState() {}
	virtual ~LLUpdateDirtyState(){ LLAppearanceMgr::getInstance()->updateIsDirty(); }
	virtual void fire(const LLUUID&) {}
};


//Inventory collect functor collecting wearables of a specific wearable type
class LLFindClothesOfType : public LLInventoryCollectFunctor
{
public:
	LLFindClothesOfType(EWearableType type) : mWearableType(type) {}
	virtual ~LLFindClothesOfType() {}
	virtual bool operator()(LLInventoryCategory* cat, LLInventoryItem* item)
	{
		if (!item) return false;
		if (item->getType() != LLAssetType::AT_CLOTHING) return false;
		
		LLViewerInventoryItem *vitem = dynamic_cast<LLViewerInventoryItem*>(item);
		if (!vitem || vitem->getWearableType() != mWearableType) return false;

		return true;
	}

	const EWearableType mWearableType;
};


LLUpdateAppearanceOnDestroy::LLUpdateAppearanceOnDestroy():
	mFireCount(0)
{
}

LLUpdateAppearanceOnDestroy::~LLUpdateAppearanceOnDestroy()
{
	llinfos << "done update appearance on destroy" << llendl;
	
	if (!LLApp::isExiting())
	{
		LLAppearanceMgr::instance().updateAppearanceFromCOF();
	}
}

void LLUpdateAppearanceOnDestroy::fire(const LLUUID& inv_item)
{
	llinfos << "callback fired" << llendl;
	mFireCount++;
}

struct LLFoundData
{
	LLFoundData() :
		mAssetType(LLAssetType::AT_NONE),
		mWearableType(WT_INVALID),
		mWearable(NULL) {}

	LLFoundData(const LLUUID& item_id,
				const LLUUID& asset_id,
				const std::string& name,
				const LLAssetType::EType& asset_type,
				const EWearableType& wearable_type
		) :
		mItemID(item_id),
		mAssetID(asset_id),
		mName(name),
		mAssetType(asset_type),
		mWearableType(wearable_type),
		mWearable( NULL ) {}
	
	LLUUID mItemID;
	LLUUID mAssetID;
	std::string mName;
	LLAssetType::EType mAssetType;
	EWearableType mWearableType;
	LLWearable* mWearable;
};

	
class LLWearableHoldingPattern
{
public:
	LLWearableHoldingPattern();
	~LLWearableHoldingPattern();

	bool pollFetchCompletion();
	void onFetchCompletion();
	bool isFetchCompleted();
	bool isTimedOut();

	void checkMissingWearables();
	bool pollMissingWearables();
	bool isMissingCompleted();
	void recoverMissingWearable(EWearableType type);
	void clearCOFLinksForMissingWearables();
	
	void onWearableAssetFetch(LLWearable *wearable);
	void onAllComplete();
	
	typedef std::list<LLFoundData> found_list_t;
	found_list_t mFoundList;
	LLInventoryModel::item_array_t mObjItems;
	LLInventoryModel::item_array_t mGestItems;
	typedef std::set<S32> type_set_t;
	type_set_t mTypesToRecover;
	type_set_t mTypesToLink;
	S32 mResolved;
	LLTimer mWaitTime;
	bool mFired;
};

LLWearableHoldingPattern::LLWearableHoldingPattern():
	mResolved(0),
	mFired(false)
{
}

LLWearableHoldingPattern::~LLWearableHoldingPattern()
{
}

bool LLWearableHoldingPattern::isFetchCompleted()
{
	return (mResolved >= (S32)mFoundList.size()); // have everything we were waiting for?
}

bool LLWearableHoldingPattern::isTimedOut()
{
	static F32 max_wait_time = 60.0;  // give up if wearable fetches haven't completed in max_wait_time seconds.
	return mWaitTime.getElapsedTimeF32() > max_wait_time; 
}

void LLWearableHoldingPattern::checkMissingWearables()
{
	std::vector<S32> found_by_type(WT_COUNT,0);
	std::vector<S32> requested_by_type(WT_COUNT,0);
	for (found_list_t::iterator it = mFoundList.begin(); it != mFoundList.end(); ++it)
	{
		LLFoundData &data = *it;
		if (data.mWearableType < WT_COUNT)
			requested_by_type[data.mWearableType]++;
		if (data.mWearable)
			found_by_type[data.mWearableType]++;
	}

	for (S32 type = 0; type < WT_COUNT; ++type)
	{
		llinfos << "type " << type << " requested " << requested_by_type[type] << " found " << found_by_type[type] << llendl;
		if (found_by_type[type] > 0)
			continue;
		if (
			// Need to recover if at least one wearable of that type
			// was requested but none was found (prevent missing
			// pants)
			(requested_by_type[type] > 0) ||  
			// or if type is a body part and no wearables were found.
			((type == WT_SHAPE) || (type == WT_SKIN) || (type == WT_HAIR) || (type == WT_EYES)))
		{
			mTypesToRecover.insert(type);
			mTypesToLink.insert(type);
			recoverMissingWearable((EWearableType)type);
			llwarns << "need to replace " << type << llendl; 
		}
	}

	mWaitTime.reset();
	if (!pollMissingWearables())
	{
		doOnIdleRepeating(boost::bind(&LLWearableHoldingPattern::pollMissingWearables,this));
	}
}

void LLWearableHoldingPattern::onAllComplete()
{
	// Activate all gestures in this folder
	if (mGestItems.count() > 0)
	{
		llinfos << "Activating " << mGestItems.count() << " gestures" << llendl;
		
		LLGestureMgr::instance().activateGestures(mGestItems);
		
		// Update the inventory item labels to reflect the fact
		// they are active.
		LLViewerInventoryCategory* catp =
			gInventory.getCategory(LLAppearanceMgr::instance().getCOF());
		
		if (catp)
		{
			gInventory.updateCategory(catp);
			gInventory.notifyObservers();
		}
	}

	// Update wearables.
	llinfos << "Updating agent wearables with " << mResolved << " wearable items " << llendl;
	LLAppearanceMgr::instance().updateAgentWearables(this, false);
	
	// Update attachments to match those requested.
	if (isAgentAvatarValid())
	{
		llinfos << "Updating " << mObjItems.count() << " attachments" << llendl;
		LLAgentWearables::userUpdateAttachments(mObjItems);
	}

	if (isFetchCompleted() && isMissingCompleted())
	{
		// Only safe to delete if all wearable callbacks and all missing wearables completed.
		delete this;
	}
}

void LLWearableHoldingPattern::onFetchCompletion()
{
	checkMissingWearables();
}

// Runs as an idle callback until all wearables are fetched (or we time out).
bool LLWearableHoldingPattern::pollFetchCompletion()
{
	bool completed = isFetchCompleted();
	bool timed_out = isTimedOut();
	bool done = completed || timed_out;

	if (done)
	{
		llinfos << "polling, done status: " << completed << " timed out " << timed_out
				<< " elapsed " << mWaitTime.getElapsedTimeF32() << llendl;

		mFired = true;
		
		if (timed_out)
		{
			llwarns << "Exceeded max wait time for wearables, updating appearance based on what has arrived" << llendl;
		}

		onFetchCompletion();
	}
	return done;
}

class RecoveredItemLinkCB: public LLInventoryCallback
{
public:
	RecoveredItemLinkCB(EWearableType type, LLWearable *wearable, LLWearableHoldingPattern* holder):
		mHolder(holder),
		mWearable(wearable),
		mType(type)
	{
	}
	void fire(const LLUUID& item_id)
	{
		llinfos << "Recovered item link for type " << mType << llendl;
		mHolder->mTypesToLink.erase(mType);
		// Add wearable to FoundData for actual wearing
		LLViewerInventoryItem *item = gInventory.getItem(item_id);
		LLViewerInventoryItem *linked_item = item ? item->getLinkedItem() : NULL;

		if (linked_item)
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, linked_item->getUUID());
			
			if (item)
			{
				LLFoundData found(linked_item->getUUID(),
						  linked_item->getAssetUUID(),
						  linked_item->getName(),
						  linked_item->getType(),
						  linked_item->isWearableType() ? linked_item->getWearableType() : WT_INVALID
						  );
				found.mWearable = mWearable;
				mHolder->mFoundList.push_front(found);
			}
			else
			{
				llwarns << "inventory item not found for recovered wearable" << llendl;
			}
		}
		else
		{
			llwarns << "inventory link not found for recovered wearable" << llendl;
		}
	}
private:
	LLWearableHoldingPattern* mHolder;
	LLWearable *mWearable;
	EWearableType mType;
};

class RecoveredItemCB: public LLInventoryCallback
{
public:
	RecoveredItemCB(EWearableType type, LLWearable *wearable, LLWearableHoldingPattern* holder):
		mHolder(holder),
		mWearable(wearable),
		mType(type)
	{
	}
	void fire(const LLUUID& item_id)
	{
		llinfos << "Recovered item for type " << mType << llendl;
		LLViewerInventoryItem *itemp = gInventory.getItem(item_id);
		mWearable->setItemID(item_id);
		LLPointer<LLInventoryCallback> cb = new RecoveredItemLinkCB(mType,mWearable,mHolder);
		mHolder->mTypesToRecover.erase(mType);
		llassert(itemp);
		if (itemp)
		{
			link_inventory_item( gAgent.getID(),
					     item_id,
					     LLAppearanceMgr::instance().getCOF(),
					     itemp->getName(),
						 itemp->getDescription(),
					     LLAssetType::AT_LINK,
					     cb);
		}
	}
private:
	LLWearableHoldingPattern* mHolder;
	LLWearable *mWearable;
	EWearableType mType;
};

void LLWearableHoldingPattern::recoverMissingWearable(EWearableType type)
{
		// Try to recover by replacing missing wearable with a new one.
	LLNotificationsUtil::add("ReplacedMissingWearable");
	lldebugs << "Wearable " << LLWearableDictionary::getTypeLabel(type)
			 << " could not be downloaded.  Replaced inventory item with default wearable." << llendl;
	LLWearable* wearable = LLWearableList::instance().createNewWearable(type);

	// Add a new one in the lost and found folder.
	const LLUUID lost_and_found_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
	LLPointer<LLInventoryCallback> cb = new RecoveredItemCB(type,wearable,this);

	create_inventory_item(gAgent.getID(),
						  gAgent.getSessionID(),
						  lost_and_found_id,
						  wearable->getTransactionID(),
						  wearable->getName(),
						  wearable->getDescription(),
						  wearable->getAssetType(),
						  LLInventoryType::IT_WEARABLE,
						  wearable->getType(),
						  wearable->getPermissions().getMaskNextOwner(),
						  cb);
}

bool LLWearableHoldingPattern::isMissingCompleted()
{
	return mTypesToLink.size()==0 && mTypesToRecover.size()==0;
}

void LLWearableHoldingPattern::clearCOFLinksForMissingWearables()
{
	for (found_list_t::iterator it = mFoundList.begin(); it != mFoundList.end(); ++it)
	{
		LLFoundData &data = *it;
		if ((data.mWearableType < WT_COUNT) && (!data.mWearable))
		{
			// Wearable link that was never resolved; remove links to it from COF
			llinfos << "removing link for unresolved item " << data.mItemID.asString() << llendl;
			LLAppearanceMgr::instance().removeCOFItemLinks(data.mItemID,false);
		}
	}
}

bool LLWearableHoldingPattern::pollMissingWearables()
{
	bool timed_out = isTimedOut();
	bool missing_completed = isMissingCompleted();
	bool done = timed_out || missing_completed;
	
	llinfos << "polling missing wearables, waiting for items " << mTypesToRecover.size()
			<< " links " << mTypesToLink.size()
			<< " wearables, timed out " << timed_out
			<< " elapsed " << mWaitTime.getElapsedTimeF32()
			<< " done " << done << llendl;

	if (done)
	{
		clearCOFLinksForMissingWearables();
		onAllComplete();
	}
	return done;
}

void LLWearableHoldingPattern::onWearableAssetFetch(LLWearable *wearable)
{
	mResolved += 1;  // just counting callbacks, not successes.
	llinfos << "onWearableAssetFetch, resolved count " << mResolved << " of requested " << mFoundList.size() << llendl;
	if (wearable)
	{
		llinfos << "wearable found, type " << wearable->getType() << " asset " << wearable->getAssetID() << llendl;
	}
	else
	{
		llwarns << "no wearable found" << llendl;
	}

	if (mFired)
	{
		llwarns << "called after holder fired" << llendl;
		return;
	}

	if (!wearable)
	{
		return;
	}

	for (LLWearableHoldingPattern::found_list_t::iterator iter = mFoundList.begin();
		 iter != mFoundList.end(); ++iter)
	{
		LLFoundData& data = *iter;
		if(wearable->getAssetID() == data.mAssetID)
		{
			data.mWearable = wearable;
			// Failing this means inventory or asset server are corrupted in a way we don't handle.
			llassert((data.mWearableType < WT_COUNT) && (wearable->getType() == data.mWearableType));
			break;
		}
	}
}

static void onWearableAssetFetch(LLWearable* wearable, void* data)
{
	LLWearableHoldingPattern* holder = (LLWearableHoldingPattern*)data;
	holder->onWearableAssetFetch(wearable);
}


static void removeDuplicateItems(LLInventoryModel::item_array_t& items)
{
	LLInventoryModel::item_array_t new_items;
	std::set<LLUUID> items_seen;
	std::deque<LLViewerInventoryItem*> tmp_list;
	// Traverse from the front and keep the first of each item
	// encountered, so we actually keep the *last* of each duplicate
	// item.  This is needed to give the right priority when adding
	// duplicate items to an existing outfit.
	for (S32 i=items.count()-1; i>=0; i--)
	{
		LLViewerInventoryItem *item = items.get(i);
		LLUUID item_id = item->getLinkedUUID();
		if (items_seen.find(item_id)!=items_seen.end())
			continue;
		items_seen.insert(item_id);
		tmp_list.push_front(item);
	}
	for (std::deque<LLViewerInventoryItem*>::iterator it = tmp_list.begin();
		 it != tmp_list.end();
		 ++it)
	{
		new_items.put(*it);
	}
	items = new_items;
}

const LLUUID LLAppearanceMgr::getCOF() const
{
	return gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT);
}


const LLViewerInventoryItem* LLAppearanceMgr::getBaseOutfitLink()
{
	const LLUUID& current_outfit_cat = getCOF();
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	// Can't search on FT_OUTFIT since links to categories return FT_CATEGORY for type since they don't
	// return preferred type.
	LLIsType is_category( LLAssetType::AT_CATEGORY ); 
	gInventory.collectDescendentsIf(current_outfit_cat,
									cat_array,
									item_array,
									false,
									is_category,
									false);
	for (LLInventoryModel::item_array_t::const_iterator iter = item_array.begin();
		 iter != item_array.end();
		 iter++)
	{
		const LLViewerInventoryItem *item = (*iter);
		const LLViewerInventoryCategory *cat = item->getLinkedCategory();
		if (cat && cat->getPreferredType() == LLFolderType::FT_OUTFIT)
		{
			return item;
		}
	}
	return NULL;
}

bool LLAppearanceMgr::getBaseOutfitName(std::string& name)
{
	const LLViewerInventoryItem* outfit_link = getBaseOutfitLink();
	if(outfit_link)
	{
		const LLViewerInventoryCategory *cat = outfit_link->getLinkedCategory();
		if (cat)
		{
			name = cat->getName();
			return true;
		}
	}
	return false;
}

const LLUUID LLAppearanceMgr::getBaseOutfitUUID()
{
	const LLViewerInventoryItem* outfit_link = getBaseOutfitLink();
	if (!outfit_link || !outfit_link->getIsLinkType()) return LLUUID::null;

	const LLViewerInventoryCategory* outfit_cat = outfit_link->getLinkedCategory();
	if (!outfit_cat) return LLUUID::null;

	if (outfit_cat->getPreferredType() != LLFolderType::FT_OUTFIT)
	{
		llwarns << "Expected outfit type:" << LLFolderType::FT_OUTFIT << " but got type:" << outfit_cat->getType() << " for folder name:" << outfit_cat->getName() << llendl;
		return LLUUID::null;
	}

	return outfit_cat->getUUID();
}

bool LLAppearanceMgr::wearItemOnAvatar(const LLUUID& item_id_to_wear, bool do_update)
{
	if (item_id_to_wear.isNull()) return false;

	//only the item from a user's inventory is allowed
	if (!gInventory.isObjectDescendentOf(item_id_to_wear, gInventory.getRootFolderID())) return false;

	LLViewerInventoryItem* item_to_wear = gInventory.getItem(item_id_to_wear);
	if (!item_to_wear) return false;

	switch (item_to_wear->getType())
	{
	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_BODYPART:
		// Don't wear anything until initial wearables are loaded, can
		// destroy clothing items.
		if (!gAgentWearables.areWearablesLoaded())
		{
			LLNotificationsUtil::add("CanNotChangeAppearanceUntilLoaded");
			return false;
		}
		addCOFItemLink(item_to_wear, do_update);
		break;
	case LLAssetType::AT_OBJECT:
		rez_attachment(item_to_wear, NULL);
		break;
	default: return false;;
	}

	return true;
}

// Update appearance from outfit folder.
void LLAppearanceMgr::changeOutfit(bool proceed, const LLUUID& category, bool append)
{
	if (!proceed)
		return;
	LLAppearanceMgr::instance().updateCOF(category,append);
}

// Create a copy of src_id + contents as a subfolder of dst_id.
void LLAppearanceMgr::shallowCopyCategory(const LLUUID& src_id, const LLUUID& dst_id,
											  LLPointer<LLInventoryCallback> cb)
{
	LLInventoryCategory *src_cat = gInventory.getCategory(src_id);
	if (!src_cat)
	{
		llwarns << "folder not found for src " << src_id.asString() << llendl;
		return;
	}
	LLUUID parent_id = dst_id;
	if(parent_id.isNull())
	{
		parent_id = gInventory.getRootFolderID();
	}
	LLUUID subfolder_id = gInventory.createNewCategory( parent_id,
														LLFolderType::FT_NONE,
														src_cat->getName());
	shallowCopyCategoryContents(src_id, subfolder_id, cb);

	gInventory.notifyObservers();
}

// Copy contents of src_id to dst_id.
void LLAppearanceMgr::shallowCopyCategoryContents(const LLUUID& src_id, const LLUUID& dst_id,
													  LLPointer<LLInventoryCallback> cb)
{
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(src_id, cats, items);
	for (LLInventoryModel::item_array_t::const_iterator iter = items->begin();
		 iter != items->end();
		 ++iter)
	{
		const LLViewerInventoryItem* item = (*iter);
		switch (item->getActualType())
		{
			case LLAssetType::AT_LINK:
			{
				//LLInventoryItem::getDescription() is used for a new description 
				//to propagate ordering information saved in descriptions of links
				link_inventory_item(gAgent.getID(),
									item->getLinkedUUID(),
									dst_id,
									item->getName(),
									item->LLInventoryItem::getDescription(),
									LLAssetType::AT_LINK, cb);
				break;
			}
			case LLAssetType::AT_LINK_FOLDER:
			{
				LLViewerInventoryCategory *catp = item->getLinkedCategory();
				// Skip copying outfit links.
				if (catp && catp->getPreferredType() != LLFolderType::FT_OUTFIT)
				{
					link_inventory_item(gAgent.getID(),
										item->getLinkedUUID(),
										dst_id,
										item->getName(),
										item->getDescription(),
										LLAssetType::AT_LINK_FOLDER, cb);
				}
				break;
			}
			case LLAssetType::AT_CLOTHING:
			case LLAssetType::AT_OBJECT:
			case LLAssetType::AT_BODYPART:
			case LLAssetType::AT_GESTURE:
			{
				copy_inventory_item(gAgent.getID(),
									item->getPermissions().getOwner(),
									item->getUUID(),
									dst_id,
									item->getName(),
									cb);
				break;
			}
			default:
				// Ignore non-outfit asset types
				break;
		}
	}
}

BOOL LLAppearanceMgr::getCanMakeFolderIntoOutfit(const LLUUID& folder_id)
{
	// These are the wearable items that are required for considering this
	// folder as containing a complete outfit.
	U32 required_wearables = 0;
	required_wearables |= 1LL << WT_SHAPE;
	required_wearables |= 1LL << WT_SKIN;
	required_wearables |= 1LL << WT_HAIR;
	required_wearables |= 1LL << WT_EYES;

	// These are the wearables that the folder actually contains.
	U32 folder_wearables = 0;
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(folder_id, cats, items);
	for (LLInventoryModel::item_array_t::const_iterator iter = items->begin();
		 iter != items->end();
		 ++iter)
	{
		const LLViewerInventoryItem* item = (*iter);
		if (item->isWearableType())
		{
			const EWearableType wearable_type = item->getWearableType();
			folder_wearables |= 1LL << wearable_type;
		}
	}

	// If the folder contains the required wearables, return TRUE.
	return ((required_wearables & folder_wearables) == required_wearables);
}


void LLAppearanceMgr::purgeBaseOutfitLink(const LLUUID& category)
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendents(category, cats, items,
								  LLInventoryModel::EXCLUDE_TRASH);
	for (S32 i = 0; i < items.count(); ++i)
	{
		LLViewerInventoryItem *item = items.get(i);
		if (item->getActualType() != LLAssetType::AT_LINK_FOLDER)
			continue;
		if (item->getIsLinkType())
		{
			LLViewerInventoryCategory* catp = item->getLinkedCategory();
			if(catp && catp->getPreferredType() == LLFolderType::FT_OUTFIT)
			{
				gInventory.purgeObject(item->getUUID());
			}
		}
	}
}

void LLAppearanceMgr::purgeCategory(const LLUUID& category, bool keep_outfit_links)
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendents(category, cats, items,
								  LLInventoryModel::EXCLUDE_TRASH);
	for (S32 i = 0; i < items.count(); ++i)
	{
		LLViewerInventoryItem *item = items.get(i);
		if (keep_outfit_links && (item->getActualType() == LLAssetType::AT_LINK_FOLDER))
			continue;
		if (item->getIsLinkType())
		{
			gInventory.purgeObject(item->getUUID());
		}
	}
}

// Keep the last N wearables of each type.  For viewer 2.0, N is 1 for
// both body parts and clothing items.
void LLAppearanceMgr::filterWearableItems(
	LLInventoryModel::item_array_t& items, S32 max_per_type)
{
	// Divvy items into arrays by wearable type.
	std::vector<LLInventoryModel::item_array_t> items_by_type(WT_COUNT);
	divvyWearablesByType(items, items_by_type);

	// rebuild items list, retaining the last max_per_type of each array
	items.clear();
	for (S32 i=0; i<WT_COUNT; i++)
	{
		S32 size = items_by_type[i].size();
		if (size <= 0)
			continue;
		S32 start_index = llmax(0,size-max_per_type);
		for (S32 j = start_index; j<size; j++)
		{
			items.push_back(items_by_type[i][j]);
		}
	}
}

// Create links to all listed items.
void LLAppearanceMgr::linkAll(const LLUUID& category,
								  LLInventoryModel::item_array_t& items,
								  LLPointer<LLInventoryCallback> cb)
{
	for (S32 i=0; i<items.count(); i++)
	{
		const LLInventoryItem* item = items.get(i).get();
		link_inventory_item(gAgent.getID(),
							item->getLinkedUUID(),
							category,
							item->getName(),
							item->LLInventoryItem::getDescription(),
							LLAssetType::AT_LINK,
							cb);
	}
}

void LLAppearanceMgr::updateCOF(const LLUUID& category, bool append)
{
	LLViewerInventoryCategory *pcat = gInventory.getCategory(category);
	llinfos << "starting, cat " << (pcat ? pcat->getName() : "[UNKNOWN]") << llendl;

	const LLUUID cof = getCOF();

	// Deactivate currently active gestures in the COF, if replacing outfit
	if (!append)
	{
		LLInventoryModel::item_array_t gest_items;
		getDescendentsOfAssetType(cof, gest_items, LLAssetType::AT_GESTURE, false);
		for(S32 i = 0; i  < gest_items.count(); ++i)
		{
			LLViewerInventoryItem *gest_item = gest_items.get(i);
			if ( LLGestureMgr::instance().isGestureActive( gest_item->getLinkedUUID()) )
			{
				LLGestureMgr::instance().deactivateGesture( gest_item->getLinkedUUID() );
			}
		}
	}
	
	// Collect and filter descendents to determine new COF contents.

	// - Body parts: always include COF contents as a fallback in case any
	// required parts are missing.
	LLInventoryModel::item_array_t body_items;
	getDescendentsOfAssetType(cof, body_items, LLAssetType::AT_BODYPART, false);
	getDescendentsOfAssetType(category, body_items, LLAssetType::AT_BODYPART, false);
	// Reduce body items to max of one per type.
	removeDuplicateItems(body_items);
	filterWearableItems(body_items, 1);

	// - Wearables: include COF contents only if appending.
	LLInventoryModel::item_array_t wear_items;
	if (append)
		getDescendentsOfAssetType(cof, wear_items, LLAssetType::AT_CLOTHING, false);
	getDescendentsOfAssetType(category, wear_items, LLAssetType::AT_CLOTHING, false);
	// Reduce wearables to max of one per type.
	removeDuplicateItems(wear_items);
	filterWearableItems(wear_items, 5);

	// - Attachments: include COF contents only if appending.
	LLInventoryModel::item_array_t obj_items;
	if (append)
		getDescendentsOfAssetType(cof, obj_items, LLAssetType::AT_OBJECT, false);
	getDescendentsOfAssetType(category, obj_items, LLAssetType::AT_OBJECT, false);
	removeDuplicateItems(obj_items);

	// - Gestures: include COF contents only if appending.
	LLInventoryModel::item_array_t gest_items;
	if (append)
		getDescendentsOfAssetType(cof, gest_items, LLAssetType::AT_GESTURE, false);
	getDescendentsOfAssetType(category, gest_items, LLAssetType::AT_GESTURE, false);
	removeDuplicateItems(gest_items);
	
	// Remove current COF contents.
	bool keep_outfit_links = append;
	purgeCategory(cof, keep_outfit_links);
	gInventory.notifyObservers();

	// Create links to new COF contents.
	llinfos << "creating LLUpdateAppearanceOnDestroy" << llendl;
	LLPointer<LLInventoryCallback> link_waiter = new LLUpdateAppearanceOnDestroy;

	linkAll(cof, body_items, link_waiter);
	linkAll(cof, wear_items, link_waiter);
	linkAll(cof, obj_items, link_waiter);
	linkAll(cof, gest_items, link_waiter);

	// Add link to outfit if category is an outfit. 
	if (!append)
	{
		createBaseOutfitLink(category, link_waiter);
	}
	llinfos << "waiting for LLUpdateAppearanceOnDestroy" << llendl;
}

void LLAppearanceMgr::updatePanelOutfitName(const std::string& name)
{
	LLSidepanelAppearance* panel_appearance =
		dynamic_cast<LLSidepanelAppearance *>(LLSideTray::getInstance()->getPanel("sidepanel_appearance"));
	if (panel_appearance)
	{
		panel_appearance->refreshCurrentOutfitName(name);
	}
}

void LLAppearanceMgr::createBaseOutfitLink(const LLUUID& category, LLPointer<LLInventoryCallback> link_waiter)
{
	const LLUUID cof = getCOF();
	LLViewerInventoryCategory* catp = gInventory.getCategory(category);
	std::string new_outfit_name = "";

	purgeBaseOutfitLink(cof);

	if (catp && catp->getPreferredType() == LLFolderType::FT_OUTFIT)
	{
		link_inventory_item(gAgent.getID(), category, cof, catp->getName(), "",
							LLAssetType::AT_LINK_FOLDER, link_waiter);
		new_outfit_name = catp->getName();
	}
	
	updatePanelOutfitName(new_outfit_name);
}

void LLAppearanceMgr::updateAgentWearables(LLWearableHoldingPattern* holder, bool append)
{
	lldebugs << "updateAgentWearables()" << llendl;
	LLInventoryItem::item_array_t items;
	LLDynamicArray< LLWearable* > wearables;

	// For each wearable type, find the first instance in the category
	// that we recursed through.
	for( S32 i = 0; i < WT_COUNT; i++ )
	{
		for (LLWearableHoldingPattern::found_list_t::iterator iter = holder->mFoundList.begin();
			 iter != holder->mFoundList.end(); ++iter)
		{
			LLFoundData& data = *iter;
			LLWearable* wearable = data.mWearable;
			if( wearable && ((S32)wearable->getType() == i) )
			{
				LLViewerInventoryItem* item;
				item = (LLViewerInventoryItem*)gInventory.getItem(data.mItemID);
				if( item && (item->getAssetUUID() == wearable->getAssetID()) )
				{
					items.put(item);
					wearables.put(wearable);
				}
			}
		}
	}

	if(wearables.count() > 0)
	{
		gAgentWearables.setWearableOutfit(items, wearables, !append);
	}

//	dec_busy_count();
}

static void remove_non_link_items(LLInventoryModel::item_array_t &items)
{
	LLInventoryModel::item_array_t pruned_items;
	for (LLInventoryModel::item_array_t::const_iterator iter = items.begin();
		 iter != items.end();
		 ++iter)
	{
 		const LLViewerInventoryItem *item = (*iter);
		if (item && item->getIsLinkType())
		{
			pruned_items.push_back((*iter));
		}
	}
	items = pruned_items;
}

//a predicate for sorting inventory items by actual descriptions
bool sort_by_description(const LLInventoryItem* item1, const LLInventoryItem* item2)
{
	if (!item1 || !item2) 
	{
		llwarning("either item1 or item2 is NULL", 0);
		return true;
	}

	return item1->LLInventoryItem::getDescription() < item2->LLInventoryItem::getDescription();
}

void LLAppearanceMgr::updateAppearanceFromCOF()
{
	//checking integrity of the COF in terms of ordering of wearables, 
	//checking and updating links' descriptions of wearables in the COF (before analyzed for "dirty" state)
	updateClothingOrderingInfo();

	// update dirty flag to see if the state of the COF matches
	// the saved outfit stored as a folder link
	llinfos << "starting" << llendl;

	updateIsDirty();

	dumpCat(getCOF(),"COF, start");

	bool follow_folder_links = true;
	LLUUID current_outfit_id = getCOF();

	// Find all the wearables that are in the COF's subtree.
	lldebugs << "LLAppearanceMgr::updateFromCOF()" << llendl;
	LLInventoryModel::item_array_t wear_items;
	LLInventoryModel::item_array_t obj_items;
	LLInventoryModel::item_array_t gest_items;
	getUserDescendents(current_outfit_id, wear_items, obj_items, gest_items, follow_folder_links);
	// Get rid of non-links in case somehow the COF was corrupted.
	remove_non_link_items(wear_items);
	remove_non_link_items(obj_items);
	remove_non_link_items(gest_items);

	if(!wear_items.count())
	{
		LLNotificationsUtil::add("CouldNotPutOnOutfit");
		return;
	}

	//preparing the list of wearables in the correct order for LLAgentWearables
	sortItemsByActualDescription(wear_items);


	LLWearableHoldingPattern* holder = new LLWearableHoldingPattern;

	holder->mObjItems = obj_items;
	holder->mGestItems = gest_items;
		
	// Note: can't do normal iteration, because if all the
	// wearables can be resolved immediately, then the
	// callback will be called (and this object deleted)
	// before the final getNextData().

	for(S32 i = 0; i  < wear_items.count(); ++i)
	{
		LLViewerInventoryItem *item = wear_items.get(i);
		LLViewerInventoryItem *linked_item = item ? item->getLinkedItem() : NULL;
		if (item && item->getIsLinkType() && linked_item)
		{
			LLFoundData found(linked_item->getUUID(),
							  linked_item->getAssetUUID(),
							  linked_item->getName(),
							  linked_item->getType(),
							  linked_item->isWearableType() ? linked_item->getWearableType() : WT_INVALID
				);

#if 0
			// Fault injection: uncomment this block to test asset
			// fetch failures (should be replaced by new defaults in
			// lost&found).
			if (found.mWearableType == WT_SHAPE || found.mWearableType == WT_JACKET)
			{
				found.mAssetID.generate(); // Replace with new UUID, guaranteed not to exist in DB
				
			}
#endif
			//pushing back, not front, to preserve order of wearables for LLAgentWearables
			holder->mFoundList.push_back(found);
		}
		else
		{
			if (!item)
			{
				llwarns << "Attempt to wear a null item " << llendl;
			}
			else if (!linked_item)
			{
				llwarns << "Attempt to wear a broken link [ name:" << item->getName() << " ] " << llendl;
			}
		}
	}

	for (LLWearableHoldingPattern::found_list_t::iterator it = holder->mFoundList.begin();
		 it != holder->mFoundList.end(); ++it)
	{
		LLFoundData& found = *it;

		llinfos << "waiting for onWearableAssetFetch callback, asset " << found.mAssetID.asString() << llendl;

		// Fetch the wearables about to be worn.
		LLWearableList::instance().getAsset(found.mAssetID,
											found.mName,
											found.mAssetType,
											onWearableAssetFetch,
											(void*)holder);

	}

	if (!holder->pollFetchCompletion())
	{
		doOnIdleRepeating(boost::bind(&LLWearableHoldingPattern::pollFetchCompletion,holder));
	}
}

void LLAppearanceMgr::getDescendentsOfAssetType(const LLUUID& category,
													LLInventoryModel::item_array_t& items,
													LLAssetType::EType type,
													bool follow_folder_links)
{
	LLInventoryModel::cat_array_t cats;
	LLIsType is_of_type(type);
	gInventory.collectDescendentsIf(category,
									cats,
									items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_of_type,
									follow_folder_links);
}

void LLAppearanceMgr::getUserDescendents(const LLUUID& category, 
											 LLInventoryModel::item_array_t& wear_items,
											 LLInventoryModel::item_array_t& obj_items,
											 LLInventoryModel::item_array_t& gest_items,
											 bool follow_folder_links)
{
	LLInventoryModel::cat_array_t wear_cats;
	LLFindWearables is_wearable;
	gInventory.collectDescendentsIf(category,
									wear_cats,
									wear_items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_wearable,
									follow_folder_links);

	LLInventoryModel::cat_array_t obj_cats;
	LLIsType is_object( LLAssetType::AT_OBJECT );
	gInventory.collectDescendentsIf(category,
									obj_cats,
									obj_items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_object,
									follow_folder_links);

	// Find all gestures in this folder
	LLInventoryModel::cat_array_t gest_cats;
	LLIsType is_gesture( LLAssetType::AT_GESTURE );
	gInventory.collectDescendentsIf(category,
									gest_cats,
									gest_items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_gesture,
									follow_folder_links);
}

void LLAppearanceMgr::wearInventoryCategory(LLInventoryCategory* category, bool copy, bool append)
{
	if(!category) return;

	llinfos << "wearInventoryCategory( " << category->getName()
			 << " )" << llendl;

	callAfterCategoryFetch(category->getUUID(),boost::bind(&LLAppearanceMgr::wearCategoryFinal,
														   &LLAppearanceMgr::instance(),
														   category->getUUID(), copy, append));
}

void LLAppearanceMgr::wearCategoryFinal(LLUUID& cat_id, bool copy_items, bool append)
{
	llinfos << "starting" << llendl;
	
	// We now have an outfit ready to be copied to agent inventory. Do
	// it, and wear that outfit normally.
	LLInventoryCategory* cat = gInventory.getCategory(cat_id);
	if(copy_items)
	{
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(cat_id, cats, items);
		std::string name;
		if(!cat)
		{
			// should never happen.
			name = "New Outfit";
		}
		else
		{
			name = cat->getName();
		}
		LLViewerInventoryItem* item = NULL;
		LLInventoryModel::item_array_t::const_iterator it = items->begin();
		LLInventoryModel::item_array_t::const_iterator end = items->end();
		LLUUID pid;
		for(; it < end; ++it)
		{
			item = *it;
			if(item)
			{
				if(LLInventoryType::IT_GESTURE == item->getInventoryType())
				{
					pid = gInventory.findCategoryUUIDForType(LLFolderType::FT_GESTURE);
				}
				else
				{
					pid = gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
				}
				break;
			}
		}
		if(pid.isNull())
		{
			pid = gInventory.getRootFolderID();
		}
		
		LLUUID new_cat_id = gInventory.createNewCategory(
			pid,
			LLFolderType::FT_NONE,
			name);
		LLPointer<LLInventoryCallback> cb = new LLWearInventoryCategoryCallback(new_cat_id, append);
		it = items->begin();
		for(; it < end; ++it)
		{
			item = *it;
			if(item)
			{
				copy_inventory_item(
					gAgent.getID(),
					item->getPermissions().getOwner(),
					item->getUUID(),
					new_cat_id,
					std::string(),
					cb);
			}
		}
		// BAP fixes a lag in display of created dir.
		gInventory.notifyObservers();
	}
	else
	{
		// Wear the inventory category.
		LLAppearanceMgr::instance().wearInventoryCategoryOnAvatar(cat, append);
	}
}

// *NOTE: hack to get from avatar inventory to avatar
void LLAppearanceMgr::wearInventoryCategoryOnAvatar( LLInventoryCategory* category, bool append )
{
	// Avoid unintentionally overwriting old wearables.  We have to do
	// this up front to avoid having to deal with the case of multiple
	// wearables being dirty.
	if(!category) return;

	llinfos << "wearInventoryCategoryOnAvatar( " << category->getName()
			 << " )" << llendl;
			 	
	if( gFloaterCustomize )
	{
		gFloaterCustomize->askToSaveIfDirty(boost::bind(&LLAppearanceMgr::changeOutfit,
														&LLAppearanceMgr::instance(),
														_1, category->getUUID(), append));
	}
	else
	{
		LLAppearanceMgr::changeOutfit(TRUE, category->getUUID(), append);
	}
}

void LLAppearanceMgr::wearOutfitByName(const std::string& name)
{
	llinfos << "Wearing category " << name << llendl;
	//inc_busy_count();

	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	LLNameCategoryCollector has_name(name);
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(),
									cat_array,
									item_array,
									LLInventoryModel::EXCLUDE_TRASH,
									has_name);
	bool copy_items = false;
	LLInventoryCategory* cat = NULL;
	if (cat_array.count() > 0)
	{
		// Just wear the first one that matches
		cat = cat_array.get(0);
	}
	else
	{
		gInventory.collectDescendentsIf(LLUUID::null,
										cat_array,
										item_array,
										LLInventoryModel::EXCLUDE_TRASH,
										has_name);
		if(cat_array.count() > 0)
		{
			cat = cat_array.get(0);
			copy_items = true;
		}
	}

	if(cat)
	{
		LLAppearanceMgr::wearInventoryCategory(cat, copy_items, false);
	}
	else
	{
		llwarns << "Couldn't find outfit " <<name<< " in wearOutfitByName()"
				<< llendl;
	}

	//dec_busy_count();
}

bool areMatchingWearables(const LLViewerInventoryItem *a, const LLViewerInventoryItem *b)
{
	return (a->isWearableType() && b->isWearableType() &&
			(a->getWearableType() == b->getWearableType()));
}

class LLDeferredCOFLinkObserver: public LLInventoryObserver
{
public:
	LLDeferredCOFLinkObserver(const LLUUID& item_id, bool do_update):
		mItemID(item_id),
		mDoUpdate(do_update)
	{
	}

	~LLDeferredCOFLinkObserver()
	{
	}
	
	/* virtual */ void changed(U32 mask)
	{
		const LLInventoryItem *item = gInventory.getItem(mItemID);
		if (item)
		{
			gInventory.removeObserver(this);
			LLAppearanceMgr::instance().addCOFItemLink(item,mDoUpdate);
			delete this;
		}
	}

private:
	const LLUUID mItemID;
	bool mDoUpdate;
};


// BAP - note that this runs asynchronously if the item is not already loaded from inventory.
// Dangerous if caller assumes link will exist after calling the function.
void LLAppearanceMgr::addCOFItemLink(const LLUUID &item_id, bool do_update )
{
	const LLInventoryItem *item = gInventory.getItem(item_id);
	if (!item)
	{
		LLDeferredCOFLinkObserver *observer = new LLDeferredCOFLinkObserver(item_id, do_update);
		gInventory.addObserver(observer);
	}
	else
	{
		addCOFItemLink(item, do_update);
	}
}

void LLAppearanceMgr::addCOFItemLink(const LLInventoryItem *item, bool do_update )
{		
	const LLViewerInventoryItem *vitem = dynamic_cast<const LLViewerInventoryItem*>(item);
	if (!vitem)
	{
		llwarns << "not an llviewerinventoryitem, failed" << llendl;
		return;
	}

	gInventory.addChangedMask(LLInventoryObserver::LABEL, vitem->getLinkedUUID());

	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	gInventory.collectDescendents(LLAppearanceMgr::getCOF(),
								  cat_array,
								  item_array,
								  LLInventoryModel::EXCLUDE_TRASH);
	bool linked_already = false;
	for (S32 i=0; i<item_array.count(); i++)
	{
		// Are these links to the same object?
		const LLViewerInventoryItem* inv_item = item_array.get(i).get();
		if (inv_item->getLinkedUUID() == vitem->getLinkedUUID())
		{
			linked_already = true;
		}
		// Are these links to different items of the same wearable
		// type? If so, new item will replace old.
		// MULTI-WEARABLES: revisit if more than one per type is allowed.
		else if (FALSE/*areMatchingWearables(vitem,inv_item)*/)
		{
			if (inv_item->getIsLinkType())
			{
				gInventory.purgeObject(inv_item->getUUID());
			}
		}
	}
	if (linked_already)
	{
		if (do_update)
		{	
			LLAppearanceMgr::updateAppearanceFromCOF();
		}
		return;
	}
	else
	{
		LLPointer<LLInventoryCallback> cb = do_update ? new ModifiedCOFCallback : 0;
		link_inventory_item( gAgent.getID(),
							 vitem->getLinkedUUID(),
							 getCOF(),
							 vitem->getName(),
							 vitem->getDescription(),
							 LLAssetType::AT_LINK,
							 cb);
	}
	return;
}

// BAP remove ensemble code for 2.1?
void LLAppearanceMgr::addEnsembleLink( LLInventoryCategory* cat, bool do_update )
{
#if SUPPORT_ENSEMBLES
	// BAP add check for already in COF.
	LLPointer<LLInventoryCallback> cb = do_update ? new ModifiedCOFCallback : 0;
	link_inventory_item( gAgent.getID(),
						 cat->getLinkedUUID(),
						 getCOF(),
						 cat->getName(),
						 cat->getDescription(),
						 LLAssetType::AT_LINK_FOLDER,
						 cb);
#endif
}

void LLAppearanceMgr::removeCOFItemLinks(const LLUUID& item_id, bool do_update)
{
	gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	gInventory.collectDescendents(LLAppearanceMgr::getCOF(),
								  cat_array,
								  item_array,
								  LLInventoryModel::EXCLUDE_TRASH);
	for (S32 i=0; i<item_array.count(); i++)
	{
		const LLInventoryItem* item = item_array.get(i).get();
		if (item->getIsLinkType() && item->getLinkedUUID() == item_id)
		{
			gInventory.purgeObject(item->getUUID());
		}
	}
	if (do_update)
	{
		LLAppearanceMgr::updateAppearanceFromCOF();
	}
}

bool sort_by_linked_uuid(const LLViewerInventoryItem* item1, const LLViewerInventoryItem* item2)
{
	if (!item1 || !item2)
	{
		llwarning("item1, item2 cannot be null, something is very wrong", 0);
		return true;
	}

	return item1->getLinkedUUID() < item2->getLinkedUUID();
}

void LLAppearanceMgr::updateIsDirty()
{
	LLUUID cof = getCOF();
	LLUUID base_outfit;

	// find base outfit link 
	const LLViewerInventoryItem* base_outfit_item = getBaseOutfitLink();
	LLViewerInventoryCategory* catp = NULL;
	if (base_outfit_item && base_outfit_item->getIsLinkType())
	{
		catp = base_outfit_item->getLinkedCategory();
	}
	if(catp && catp->getPreferredType() == LLFolderType::FT_OUTFIT)
	{
		base_outfit = catp->getUUID();
	}

	if(base_outfit.isNull())
	{
		// no outfit link found, display "unsaved outfit"
		mOutfitIsDirty = true;
	}
	else
	{
		LLInventoryModel::cat_array_t cof_cats;
		LLInventoryModel::item_array_t cof_items;
		gInventory.collectDescendents(cof, cof_cats, cof_items,
									  LLInventoryModel::EXCLUDE_TRASH);

		LLInventoryModel::cat_array_t outfit_cats;
		LLInventoryModel::item_array_t outfit_items;
		gInventory.collectDescendents(base_outfit, outfit_cats, outfit_items,
									  LLInventoryModel::EXCLUDE_TRASH);

		if(outfit_items.count() != cof_items.count() -1)
		{
			// Current outfit folder should have one more item than the outfit folder.
			// this one item is the link back to the outfit folder itself.
			mOutfitIsDirty = true;
			return;
		}

		//getting rid of base outfit folder link to simplify comparison
		for (LLInventoryModel::item_array_t::iterator it = cof_items.begin(); it != cof_items.end(); ++it)
		{
			if (*it == base_outfit_item)
			{
				cof_items.erase(it);
				break;
			}
		}

		//"dirty" - also means a difference in linked UUIDs and/or a difference in wearables order (links' descriptions)
		std::sort(cof_items.begin(), cof_items.end(), sort_by_linked_uuid);
		std::sort(outfit_items.begin(), outfit_items.end(), sort_by_linked_uuid);

		for (U32 i = 0; i < cof_items.size(); ++i)
		{
			LLViewerInventoryItem *item1 = cof_items.get(i);
			LLViewerInventoryItem *item2 = outfit_items.get(i);

			if (item1->getLinkedUUID() != item2->getLinkedUUID() || 
				item1->LLInventoryItem::getDescription() != item2->LLInventoryItem::getDescription())
			{
				mOutfitIsDirty = true;
				return;
			}
		}

		mOutfitIsDirty = false;
	}
}

void LLAppearanceMgr::autopopulateOutfits()
{
	// If this is the very first time the user has logged into viewer2+ (from a legacy viewer, or new account)
	// then auto-populate outfits from the library into the My Outfits folder.

	llinfos << "avatar fully visible" << llendl;

	static bool check_populate_my_outfits = true;
	if (check_populate_my_outfits && 
		(LLInventoryModel::getIsFirstTimeInViewer2() 
		 || gSavedSettings.getBOOL("MyOutfitsAutofill")))
	{
		gAgentWearables.populateMyOutfitsFolder();
	}
	check_populate_my_outfits = false;
}

// Handler for anything that's deferred until avatar de-clouds.
void LLAppearanceMgr::onFirstFullyVisible()
{
	autopopulateOutfits();
}

bool LLAppearanceMgr::updateBaseOutfit()
{
	const LLUUID base_outfit_id = getBaseOutfitUUID();
	if (base_outfit_id.isNull()) return false;

	updateClothingOrderingInfo();

	// in a Base Outfit we do not remove items, only links
	purgeCategory(base_outfit_id, false);


	LLPointer<LLInventoryCallback> dirty_state_updater = new LLUpdateDirtyState();

	//COF contains only links so we copy to the Base Outfit only links
	shallowCopyCategoryContents(getCOF(), base_outfit_id, dirty_state_updater);

	return true;
}

void LLAppearanceMgr::divvyWearablesByType(const LLInventoryModel::item_array_t& items, wearables_by_type_t& items_by_type)
{
	items_by_type.reserve(WT_COUNT);
	if (items.empty()) return;

	for (S32 i=0; i<items.count(); i++)
	{
		LLViewerInventoryItem *item = items.get(i);
		// Ignore non-wearables.
		if (!item->isWearableType())
			continue;
		EWearableType type = item->getWearableType();
		if(type < 0 || type >= WT_COUNT)
		{
			LL_WARNS("Appearance") << "Invalid wearable type. Inventory type does not match wearable flag bitfield." << LL_ENDL;
			continue;
		}
		items_by_type[type].push_back(item);
	}
}

std::string build_order_string(EWearableType type, U32 i)
{
		std::ostringstream order_num;
		order_num << ORDER_NUMBER_SEPARATOR << type * 100 + i;
		return order_num.str();
}

struct WearablesOrderComparator
{
	WearablesOrderComparator(const EWearableType type)
	{
		mControlSize = build_order_string(type, 0).size();
	};

	bool operator()(const LLInventoryItem* item1, const LLInventoryItem* item2)
	{
		if (!item1 || !item2)
		{
			llwarning("either item1 or item2 is NULL", 0);
			return true;
		}
		
		const std::string& desc1 = item1->LLInventoryItem::getDescription();
		const std::string& desc2 = item2->LLInventoryItem::getDescription();
		
		bool item1_valid = (desc1.size() == mControlSize) && (ORDER_NUMBER_SEPARATOR == desc1[0]);
		bool item2_valid = (desc2.size() == mControlSize) && (ORDER_NUMBER_SEPARATOR == desc2[0]);

		if (item1_valid && item2_valid)
			return desc1 < desc2;

		//we need to sink down invalid items: items with empty descriptions, items with "Broken link" descriptions,
		//items with ordering information but not for the associated wearables type
		if (!item1_valid && item2_valid) 
			return false;

		return true;
	}

	U32 mControlSize;
};

void LLAppearanceMgr::updateClothingOrderingInfo()
{
	LLInventoryModel::item_array_t wear_items;
	getDescendentsOfAssetType(getCOF(), wear_items, LLAssetType::AT_CLOTHING, false);

	wearables_by_type_t items_by_type(WT_COUNT);
	divvyWearablesByType(wear_items, items_by_type);

	bool inventory_changed = false;
	for (U32 type = WT_SHIRT; type < WT_COUNT; type++)
	{
		
		U32 size = items_by_type[type].size();
		if (!size) continue;

		//sinking down invalid items which need reordering
		std::sort(items_by_type[type].begin(), items_by_type[type].end(), WearablesOrderComparator((EWearableType) type));

		//requesting updates only for those links which don't have "valid" descriptions
		for (U32 i = 0; i < size; i++)
		{
			LLViewerInventoryItem* item = items_by_type[type][i];
			if (!item) continue;

			std::string new_order_str = build_order_string((EWearableType)type, i);
			if (new_order_str == item->LLInventoryItem::getDescription()) continue;

			item->setDescription(new_order_str);
			item->setComplete(TRUE);
 			item->updateServer(FALSE);
			gInventory.updateItem(item);
			inventory_changed = true;
		}
	}

	//*TODO do we really need to notify observers?
	if (inventory_changed) gInventory.notifyObservers();
}




class LLShowCreatedOutfit: public LLInventoryCallback
{
public:
	LLShowCreatedOutfit(LLUUID& folder_id): mFolderID(folder_id)
	{}

	virtual ~LLShowCreatedOutfit()
	{
		LLSD key;
		LLSideTray::getInstance()->showPanel("panel_outfits_inventory", key);
		LLPanelOutfitsInventory *outfit_panel =
			dynamic_cast<LLPanelOutfitsInventory*>(LLSideTray::getInstance()->getPanel("panel_outfits_inventory"));
		if (outfit_panel)
		{
			outfit_panel->getRootFolder()->clearSelection();
			outfit_panel->getRootFolder()->setSelectionByID(mFolderID, TRUE);
		}
		
		LLAccordionCtrlTab* tab_outfits = outfit_panel ? outfit_panel->findChild<LLAccordionCtrlTab>("tab_outfits") : 0;
		if (tab_outfits && !tab_outfits->getDisplayChildren())
		{
			tab_outfits->changeOpenClose(tab_outfits->getDisplayChildren());
		}

		LLAppearanceMgr::getInstance()->updateIsDirty();
		LLAppearanceMgr::getInstance()->updatePanelOutfitName("");
	}

	virtual void fire(const LLUUID&)
	{}

private:
	LLUUID mFolderID;
};

LLUUID LLAppearanceMgr::makeNewOutfitLinks(const std::string& new_folder_name)
{
	if (!isAgentAvatarValid()) return LLUUID::null;

	// First, make a folder in the My Outfits directory.
	const LLUUID parent_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_MY_OUTFITS);
	LLUUID folder_id = gInventory.createNewCategory(
		parent_id,
		LLFolderType::FT_OUTFIT,
		new_folder_name);

	updateClothingOrderingInfo();

	LLPointer<LLInventoryCallback> cb = new LLShowCreatedOutfit(folder_id);
	shallowCopyCategoryContents(getCOF(),folder_id, cb);
	createBaseOutfitLink(folder_id, cb);

	dumpCat(folder_id,"COF, new outfit");

	return folder_id;
}

void LLAppearanceMgr::wearBaseOutfit()
{
	const LLUUID& base_outfit_id = getBaseOutfitUUID();
	if (base_outfit_id.isNull()) return;
	
	updateCOF(base_outfit_id);
}

void LLAppearanceMgr::removeItemFromAvatar(const LLUUID& id_to_remove)
{
	LLViewerInventoryItem * item_to_remove = gInventory.getItem(id_to_remove);
	if (!item_to_remove) return;

	switch (item_to_remove->getType())
	{
	case LLAssetType::AT_CLOTHING:
		if (get_is_item_worn(id_to_remove))
		{
			//*TODO move here the exact removing code from LLWearableBridge::removeItemFromAvatar in the future
			LLWearableBridge::removeItemFromAvatar(item_to_remove);
		}
		break;
	case LLAssetType::AT_OBJECT:
		gMessageSystem->newMessageFast(_PREHASH_DetachAttachmentIntoInv);
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_ItemID, item_to_remove->getLinkedUUID());
		gMessageSystem->sendReliable( gAgent.getRegion()->getHost());

		{
			// this object might have been selected, so let the selection manager know it's gone now
			LLViewerObject *found_obj = gObjectList.findObject(item_to_remove->getLinkedUUID());
			if (found_obj)
			{
				LLSelectMgr::getInstance()->remove(found_obj);
			};
		}
	default: break;
	}
}


bool LLAppearanceMgr::moveWearable(LLViewerInventoryItem* item, bool closer_to_body)
{
	if (!item || !item->isWearableType()) return false;
	if (item->getType() != LLAssetType::AT_CLOTHING) return false;
	if (!gInventory.isObjectDescendentOf(item->getUUID(), getCOF())) return false;

	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLFindClothesOfType filter_wearables_of_type(item->getWearableType());
	gInventory.collectDescendentsIf(getCOF(), cats, items, true, filter_wearables_of_type);
	if (items.empty()) return false;

	//*TODO all items are not guarantied to have valid descriptions (check?)
	std::sort(items.begin(), items.end(), WearablesOrderComparator(item->getWearableType()));

	if (closer_to_body && items.front() == item) return false;
	if (!closer_to_body && items.back() == item) return false;
	
	LLInventoryModel::item_array_t::iterator it = std::find(items.begin(), items.end(), item);
	if (items.end() == it) return false;


	//swapping descriptions
	closer_to_body ? --it : ++it;
	LLViewerInventoryItem* swap_item = *it;
	if (!swap_item) return false;
	std::string tmp = swap_item->LLInventoryItem::getDescription();
	swap_item->setDescription(item->LLInventoryItem::getDescription());
	item->setDescription(tmp);


	//items need to be updated on a dataserver
	item->setComplete(TRUE);
	item->updateServer(FALSE);
	gInventory.updateItem(item);

	swap_item->setComplete(TRUE);
	swap_item->updateServer(FALSE);
	gInventory.updateItem(swap_item);

	//to cause appearance of the agent to be updated
	bool result = false;
	if (result = gAgentWearables.moveWearable(item, closer_to_body))
	{
		gAgentAvatarp->wearableUpdated(item->getWearableType(), TRUE);
	}

	setOutfitDirty(true);

	//*TODO do we need to notify observers here in such a way?
	gInventory.notifyObservers();

	return result;
}

//static
void LLAppearanceMgr::sortItemsByActualDescription(LLInventoryModel::item_array_t& items)
{
	if (items.size() < 2) return;

	std::sort(items.begin(), items.end(), sort_by_description);
}

//#define DUMP_CAT_VERBOSE

void LLAppearanceMgr::dumpCat(const LLUUID& cat_id, const std::string& msg)
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendents(cat_id, cats, items, LLInventoryModel::EXCLUDE_TRASH);

#ifdef DUMP_CAT_VERBOSE
	llinfos << llendl;
	llinfos << str << llendl;
	S32 hitcount = 0;
	for(S32 i=0; i<items.count(); i++)
	{
		LLViewerInventoryItem *item = items.get(i);
		if (item)
			hitcount++;
		llinfos << i <<" "<< item->getName() <<llendl;
	}
#endif
	llinfos << msg << " count " << items.count() << llendl;
}

void LLAppearanceMgr::dumpItemArray(const LLInventoryModel::item_array_t& items,
										const std::string& msg)
{
	llinfos << msg << llendl;
	for (S32 i=0; i<items.count(); i++)
	{
		LLViewerInventoryItem *item = items.get(i);
		llinfos << i <<" " << item->getName() << llendl;
	}
	llinfos << llendl;
}

LLAppearanceMgr::LLAppearanceMgr():
	mAttachmentInvLinkEnabled(false),
	mOutfitIsDirty(false)
{
}

LLAppearanceMgr::~LLAppearanceMgr()
{
}

void LLAppearanceMgr::setAttachmentInvLinkEnable(bool val)
{
	llinfos << "setAttachmentInvLinkEnable => " << (int) val << llendl;
	mAttachmentInvLinkEnabled = val;
}

// BAP TODO - mRegisteredAttachments is currently maintained but not used for anything.  Consider yanking.
void dumpAttachmentSet(const std::set<LLUUID>& atts, const std::string& msg)
{
       llinfos << msg << llendl;
       for (std::set<LLUUID>::const_iterator it = atts.begin();
               it != atts.end();
               ++it)
       {
               LLUUID item_id = *it;
               LLViewerInventoryItem *item = gInventory.getItem(item_id);
               if (item)
                       llinfos << "atts " << item->getName() << llendl;
               else
                       llinfos << "atts " << "UNKNOWN[" << item_id.asString() << "]" << llendl;
       }
       llinfos << llendl;
}

void LLAppearanceMgr::registerAttachment(const LLUUID& item_id)
{
       mRegisteredAttachments.insert(item_id);
	   gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

	   if (mAttachmentInvLinkEnabled)
	   {
		   LLAppearanceMgr::addCOFItemLink(item_id, false);  // Add COF link for item.
	   }
	   else
	   {
		   //llinfos << "no link changes, inv link not enabled" << llendl;
	   }
}

void LLAppearanceMgr::unregisterAttachment(const LLUUID& item_id)
{
       mRegisteredAttachments.erase(item_id);
	   gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

	   if (mAttachmentInvLinkEnabled)
	   {
		   LLAppearanceMgr::removeCOFItemLinks(item_id, false);
	   }
	   else
	   {
		   //llinfos << "no link changes, inv link not enabled" << llendl;
	   }
}

void LLAppearanceMgr::linkRegisteredAttachments()
{
	for (std::set<LLUUID>::iterator it = mRegisteredAttachments.begin();
		 it != mRegisteredAttachments.end();
		 ++it)
	{
		LLUUID item_id = *it;
		addCOFItemLink(item_id, false);
	}
	mRegisteredAttachments.clear();
}

BOOL LLAppearanceMgr::getIsInCOF(const LLUUID& obj_id) const
{
	return gInventory.isObjectDescendentOf(obj_id, getCOF());
}

BOOL LLAppearanceMgr::getIsProtectedCOFItem(const LLUUID& obj_id) const
{
	if (!getIsInCOF(obj_id)) return FALSE;

	// If a non-link somehow ended up in COF, allow deletion.
	const LLInventoryObject *obj = gInventory.getObject(obj_id);
	if (obj && !obj->getIsLinkType())
	{
		return FALSE;
	}

	// For now, don't allow direct deletion from the COF.  Instead, force users
	// to choose "Detach" or "Take Off".
	return TRUE;
	/*
	const LLInventoryObject *obj = gInventory.getObject(obj_id);
	if (!obj) return FALSE;

	// Can't delete bodyparts, since this would be equivalent to removing the item.
	if (obj->getType() == LLAssetType::AT_BODYPART) return TRUE;

	// Can't delete the folder link, since this is saved for bookkeeping.
	if (obj->getActualType() == LLAssetType::AT_LINK_FOLDER) return TRUE;

	return FALSE;
	*/
}
