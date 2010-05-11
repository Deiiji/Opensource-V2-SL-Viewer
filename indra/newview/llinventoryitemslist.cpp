/**
 * @file llinventoryitemslist.cpp
 * @brief A list of inventory items represented by LLFlatListView.
 *
 * Class LLInventoryItemsList implements a flat list of inventory items.
 * Class LLPanelInventoryListItem displays inventory item as an element
 * of LLInventoryItemsList.
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 * 
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "llinventoryitemslist.h"

// llcommon
#include "llcommonutils.h"

#include "lliconctrl.h"

#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "lltextutil.h"
#include "lltrans.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static const S32 WIDGET_SPACING = 3;

LLPanelInventoryListItemBase* LLPanelInventoryListItemBase::create(LLViewerInventoryItem* item)
{
	LLPanelInventoryListItemBase* list_item = NULL;
	if (item)
	{
		list_item = new LLPanelInventoryListItemBase(item);
		list_item->init();
	}
	return list_item;
}

void LLPanelInventoryListItemBase::draw()
{
	if (getNeedsRefresh())
	{
		updateItem();
		setNeedsRefresh(false);
	}
	LLPanel::draw();
}

void LLPanelInventoryListItemBase::updateItem()
{
	setIconImage(mIconImage);
	setTitle(mItem->getName(), mHighlightedText);
}

void LLPanelInventoryListItemBase::addWidgetToLeftSide(const std::string& name, bool show_widget/* = true*/)
{
	LLUICtrl* ctrl = findChild<LLUICtrl>(name);
	if(ctrl)
	{
		addWidgetToLeftSide(ctrl, show_widget);
	}
}

void LLPanelInventoryListItemBase::addWidgetToLeftSide(LLUICtrl* ctrl, bool show_widget/* = true*/)
{
	mLeftSideWidgets.push_back(ctrl);
	setShowWidget(ctrl, show_widget);
}

void LLPanelInventoryListItemBase::addWidgetToRightSide(const std::string& name, bool show_widget/* = true*/)
{
	LLUICtrl* ctrl = findChild<LLUICtrl>(name);
	if(ctrl)
	{
		addWidgetToRightSide(ctrl, show_widget);
	}
}

void LLPanelInventoryListItemBase::addWidgetToRightSide(LLUICtrl* ctrl, bool show_widget/* = true*/)
{
	mRightSideWidgets.push_back(ctrl);
	setShowWidget(ctrl, show_widget);
}

void LLPanelInventoryListItemBase::setShowWidget(const std::string& name, bool show)
{
	LLUICtrl* widget = findChild<LLUICtrl>(name);
	if(widget)
	{
		setShowWidget(widget, show);
	}
}

void LLPanelInventoryListItemBase::setShowWidget(LLUICtrl* ctrl, bool show)
{
	// Enable state determines whether widget may become visible in setWidgetsVisible()
	ctrl->setEnabled(show);
}

BOOL LLPanelInventoryListItemBase::postBuild()
{
	setIconCtrl(getChild<LLIconCtrl>("item_icon"));
	setTitleCtrl(getChild<LLTextBox>("item_name"));

	mIconImage = get_item_icon(mItem->getType(), mItem->getInventoryType(), mItem->getFlags(), FALSE);

	setNeedsRefresh(true);

	setWidgetsVisible(false);
	reshapeWidgets();

	return TRUE;
}

void LLPanelInventoryListItemBase::setValue(const LLSD& value)
{
	if (!value.isMap()) return;
	if (!value.has("selected")) return;
	childSetVisible("selected_icon", value["selected"]);
}

void LLPanelInventoryListItemBase::onMouseEnter(S32 x, S32 y, MASK mask)
{
	childSetVisible("hovered_icon", true);
	LLPanel::onMouseEnter(x, y, mask);
}

void LLPanelInventoryListItemBase::onMouseLeave(S32 x, S32 y, MASK mask)
{
	childSetVisible("hovered_icon", false);
	LLPanel::onMouseLeave(x, y, mask);
}

S32 LLPanelInventoryListItemBase::notify(const LLSD& info)
{
	S32 rv = 0;
	if(info.has("match_filter"))
	{
		mHighlightedText = info["match_filter"].asString();

		std::string test(mItem->getName());
		LLStringUtil::toUpper(test);

		if(mHighlightedText.empty() || std::string::npos != test.find(mHighlightedText))
		{
			rv = 0; // substring is found
		}
		else
		{
			rv = -1;
		}

		setNeedsRefresh(true);
	}
	else
	{
		rv = LLPanel::notify(info);
	}
	return rv;
}

LLPanelInventoryListItemBase::LLPanelInventoryListItemBase(LLViewerInventoryItem* item)
: LLPanel()
, mItem(item)
, mIconCtrl(NULL)
, mTitleCtrl(NULL)
, mWidgetSpacing(WIDGET_SPACING)
, mLeftWidgetsWidth(0)
, mRightWidgetsWidth(0)
, mNeedsRefresh(false)
{
}

void LLPanelInventoryListItemBase::init()
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_inventory_item.xml");
}

class WidgetVisibilityChanger
{
public:
	WidgetVisibilityChanger(bool visible) : mVisible(visible){}
	void operator()(LLUICtrl* widget)
	{
		// Disabled widgets never become visible. see LLPanelInventoryListItemBase::setShowWidget()
		widget->setVisible(mVisible && widget->getEnabled());
	}
private:
	bool mVisible;
};

void LLPanelInventoryListItemBase::setWidgetsVisible(bool visible)
{
	std::for_each(mLeftSideWidgets.begin(), mLeftSideWidgets.end(), WidgetVisibilityChanger(visible));
	std::for_each(mRightSideWidgets.begin(), mRightSideWidgets.end(), WidgetVisibilityChanger(visible));
}

void LLPanelInventoryListItemBase::reshapeWidgets()
{
	// disabled reshape left for now to reserve space for 'delete' button in LLPanelClothingListItem
	/*reshapeLeftWidgets();*/
	reshapeRightWidgets();
	reshapeMiddleWidgets();
}

void LLPanelInventoryListItemBase::setIconImage(const LLUIImagePtr& image)
{
	if(image)
	{
		mIconImage = image; 
		mIconCtrl->setImage(mIconImage);
	}
}

void LLPanelInventoryListItemBase::setTitle(const std::string& title, const std::string& highlit_text)
{
	LLTextUtil::textboxSetHighlightedVal(
		mTitleCtrl,
		LLStyle::Params(),
		title,
		highlit_text);
}

void LLPanelInventoryListItemBase::reshapeLeftWidgets()
{
	S32 widget_left = 0;
	mLeftWidgetsWidth = 0;

	widget_array_t::const_iterator it = mLeftSideWidgets.begin();
	const widget_array_t::const_iterator it_end = mLeftSideWidgets.end();
	for( ; it_end != it; ++it)
	{
		LLUICtrl* widget = *it;
		if(!widget->getVisible())
		{
			continue;
		}
		LLRect widget_rect(widget->getRect());
		widget_rect.setLeftTopAndSize(widget_left, widget_rect.mTop, widget_rect.getWidth(), widget_rect.getHeight());
		widget->setShape(widget_rect);

		widget_left += widget_rect.getWidth() + getWidgetSpacing();
		mLeftWidgetsWidth = widget_rect.mRight;
	}
}

void LLPanelInventoryListItemBase::reshapeRightWidgets()
{
	S32 widget_right = getLocalRect().getWidth();
	S32 widget_left = widget_right;

	widget_array_t::const_reverse_iterator it = mRightSideWidgets.rbegin();
	const widget_array_t::const_reverse_iterator it_end = mRightSideWidgets.rend();
	for( ; it_end != it; ++it)
	{
		LLUICtrl* widget = *it;
		if(!widget->getVisible())
		{
			continue;
		}
		LLRect widget_rect(widget->getRect());
		widget_left = widget_right - widget_rect.getWidth();
		widget_rect.setLeftTopAndSize(widget_left, widget_rect.mTop, widget_rect.getWidth(), widget_rect.getHeight());
		widget->setShape(widget_rect);

		widget_right = widget_left - getWidgetSpacing();
	}
	mRightWidgetsWidth = getLocalRect().getWidth() - widget_left;
}

void LLPanelInventoryListItemBase::reshapeMiddleWidgets()
{
	LLRect icon_rect(mIconCtrl->getRect());
	icon_rect.setLeftTopAndSize(mLeftWidgetsWidth + getWidgetSpacing(), icon_rect.mTop, 
		icon_rect.getWidth(), icon_rect.getHeight());
	mIconCtrl->setShape(icon_rect);

	S32 name_left = icon_rect.mRight + getWidgetSpacing();
	S32 name_right = getLocalRect().getWidth() - mRightWidgetsWidth - getWidgetSpacing();
	LLRect name_rect(mTitleCtrl->getRect());
	name_rect.set(name_left, name_rect.mTop, name_right, name_rect.mBottom);
	mTitleCtrl->setShape(name_rect);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

LLInventoryItemsList::Params::Params()
{}

LLInventoryItemsList::LLInventoryItemsList(const LLInventoryItemsList::Params& p)
:	LLFlatListViewEx(p)
,	mNeedsRefresh(false)
{
	// TODO: mCommitOnSelectionChange is set to "false" in LLFlatListView
	// but reset to true in all derived classes. This settings might need to
	// be added to LLFlatListView::Params() and/or set to "true" by default.
	setCommitOnSelectionChange(true);

	setNoFilteredItemsMsg(LLTrans::getString("InventoryNoMatchingItems"));
}

// virtual
LLInventoryItemsList::~LLInventoryItemsList()
{}

void LLInventoryItemsList::refreshList(const LLInventoryModel::item_array_t item_array)
{
	getIDs().clear();
	LLInventoryModel::item_array_t::const_iterator it = item_array.begin();
	for( ; item_array.end() != it; ++it)
	{
		getIDs().push_back((*it)->getUUID());
	}
	mNeedsRefresh = true;
}

void LLInventoryItemsList::draw()
{
	LLFlatListViewEx::draw();
	if(mNeedsRefresh)
	{
		refresh();
	}
}

void LLInventoryItemsList::refresh()
{
	static const unsigned ADD_LIMIT = 50;

	uuid_vec_t added_items;
	uuid_vec_t removed_items;

	computeDifference(getIDs(), added_items, removed_items);

	bool add_limit_exceeded = false;
	unsigned nadded = 0;

	uuid_vec_t::const_iterator it = added_items.begin();
	for( ; added_items.end() != it; ++it)
	{
		if(nadded >= ADD_LIMIT)
		{
			add_limit_exceeded = true;
			break;
		}
		LLViewerInventoryItem* item = gInventory.getItem(*it);
		// Do not rearrange items on each adding, let's do that on filter call
		addNewItem(item, false);
		++nadded;
	}

	it = removed_items.begin();
	for( ; removed_items.end() != it; ++it)
	{
		removeItemByUUID(*it);
	}

	// Filter, rearrange and notify parent about shape changes
	filterItems();

	bool needs_refresh = add_limit_exceeded;
	setNeedsRefresh(needs_refresh);
}

void LLInventoryItemsList::computeDifference(
	const uuid_vec_t& vnew,
	uuid_vec_t& vadded,
	uuid_vec_t& vremoved)
{
	uuid_vec_t vcur;
	{
		std::vector<LLSD> vcur_values;
		getValues(vcur_values);

		for (size_t i=0; i<vcur_values.size(); i++)
			vcur.push_back(vcur_values[i].asUUID());
	}

	LLCommonUtils::computeDifference(vnew, vcur, vadded, vremoved);
}

void LLInventoryItemsList::addNewItem(LLViewerInventoryItem* item, bool rearrange /*= true*/)
{
	if (!item)
	{
		llwarns << "No inventory item. Couldn't create flat list item." << llendl;
		llassert(item != NULL);
	}

	LLPanelInventoryListItemBase *list_item = LLPanelInventoryListItemBase::create(item);
	if (!list_item)
		return;

	bool is_item_added = addItem(list_item, item->getUUID(), ADD_BOTTOM, rearrange);
	if (!is_item_added)
	{
		llwarns << "Couldn't add flat list item." << llendl;
		llassert(is_item_added);
	}
}

// EOF
