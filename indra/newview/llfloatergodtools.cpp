/** 
 * @file llfloatergodtools.cpp
 * @brief The on-screen rectangle with tool options.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2010, Linden Research, Inc.
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

#include "llfloatergodtools.h"

#include "llcoord.h"
#include "llfontgl.h"
#include "llframetimer.h"
#include "llgl.h"
#include "llhost.h"
#include "llnotificationsutil.h"
#include "llregionflags.h"
#include "llstring.h"
#include "message.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldraghandle.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llfloatertopobjects.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "llspinctrl.h"
#include "llstatusbar.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrl.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "llfloateravatarpicker.h"
#include "llxfermanager.h"
#include "llvlcomposition.h"
#include "llsurface.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "lltrans.h"

#include "lltransfertargetfile.h"
#include "lltransfersourcefile.h"

const F32 SECONDS_BETWEEN_UPDATE_REQUESTS = 5.0f;

//*****************************************************************************
// LLFloaterGodTools
//*****************************************************************************

void LLFloaterGodTools::onOpen(const LLSD& key)
{
	center();
	setFocus(TRUE);
// 	LLPanel *panel = childGetVisibleTab("GodTools Tabs");
// 	if (panel)
// 		panel->setFocus(TRUE);
	if (mPanelObjectTools)
		mPanelObjectTools->setTargetAvatar(LLUUID::null);

	if (gAgent.getRegionHost() != mCurrentHost)
	{
		// we're in a new region
		sendRegionInfoRequest();
	}
}
 

// static
void LLFloaterGodTools::refreshAll()
{
	LLFloaterGodTools* god_tools = LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools");
	if (god_tools)
	{
		if (gAgent.getRegionHost() != god_tools->mCurrentHost)
		{
			// we're in a new region
			god_tools->sendRegionInfoRequest();
		}
	}
}



LLFloaterGodTools::LLFloaterGodTools(const LLSD& key)
:	LLFloater(key),
	mCurrentHost(LLHost::invalid),
	mUpdateTimer()
{
	mFactoryMap["grid"] = LLCallbackMap(createPanelGrid, this);
	mFactoryMap["region"] = LLCallbackMap(createPanelRegion, this);
	mFactoryMap["objects"] = LLCallbackMap(createPanelObjects, this);
	mFactoryMap["request"] = LLCallbackMap(createPanelRequest, this);
//	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_god_tools.xml");

}

BOOL LLFloaterGodTools::postBuild()
{
	sendRegionInfoRequest();
	childShowTab("GodTools Tabs", "region");
	return TRUE;
}
// static
void* LLFloaterGodTools::createPanelGrid(void *userdata)
{
	return new LLPanelGridTools();
}

// static
void* LLFloaterGodTools::createPanelRegion(void *userdata)
{
	LLFloaterGodTools* self = (LLFloaterGodTools*)userdata;
	self->mPanelRegionTools = new LLPanelRegionTools();
	return self->mPanelRegionTools;
}

// static
void* LLFloaterGodTools::createPanelObjects(void *userdata)
{
	LLFloaterGodTools* self = (LLFloaterGodTools*)userdata;
	self->mPanelObjectTools = new LLPanelObjectTools();
	return self->mPanelObjectTools;
}

// static
void* LLFloaterGodTools::createPanelRequest(void *userdata)
{
	return new LLPanelRequestTools();
}

LLFloaterGodTools::~LLFloaterGodTools()
{
	// children automatically deleted
}


U32 LLFloaterGodTools::computeRegionFlags() const
{
	U32 flags = gAgent.getRegion()->getRegionFlags();
	if (mPanelRegionTools) flags = mPanelRegionTools->computeRegionFlags(flags);
	if (mPanelObjectTools) flags = mPanelObjectTools->computeRegionFlags(flags);
	return flags;
}


void LLFloaterGodTools::updatePopup(LLCoordGL center, MASK mask)
{
}

// virtual
void LLFloaterGodTools::draw()
{
	if (mCurrentHost == LLHost::invalid)
	{
		if (mUpdateTimer.getElapsedTimeF32() > SECONDS_BETWEEN_UPDATE_REQUESTS)
		{
			sendRegionInfoRequest();
		}
	}
	else if (gAgent.getRegionHost() != mCurrentHost)
	{
		sendRegionInfoRequest();
	}
	LLFloater::draw();
}

void LLFloaterGodTools::showPanel(const std::string& panel_name)
{
	childShowTab("GodTools Tabs", panel_name);
	openFloater();
	LLPanel *panel = childGetVisibleTab("GodTools Tabs");
	if (panel)
		panel->setFocus(TRUE);
}

// static
void LLFloaterGodTools::processRegionInfo(LLMessageSystem* msg)
{
	llassert(msg);
	if (!msg) return;

	LLHost host = msg->getSender();
	if (host != gAgent.getRegionHost())
	{
		// update is for a different region than the one we're in
		return;
	}

	//const S32 SIM_NAME_BUF = 256;
	U32 region_flags;
	U8 sim_access;
	U8 agent_limit;
	std::string sim_name;
	U32 estate_id;
	U32 parent_estate_id;
	F32 water_height;
	F32 billable_factor;
	F32 object_bonus_factor;
	F32 terrain_raise_limit;
	F32 terrain_lower_limit;
	S32 price_per_meter;
	S32 redirect_grid_x;
	S32 redirect_grid_y;
	LLUUID cache_id;

	msg->getStringFast(_PREHASH_RegionInfo, _PREHASH_SimName, sim_name);
	msg->getU32Fast(_PREHASH_RegionInfo, _PREHASH_EstateID, estate_id);
	msg->getU32Fast(_PREHASH_RegionInfo, _PREHASH_ParentEstateID, parent_estate_id);
	msg->getU32Fast(_PREHASH_RegionInfo, _PREHASH_RegionFlags, region_flags);
	msg->getU8Fast(_PREHASH_RegionInfo, _PREHASH_SimAccess, sim_access);
	msg->getU8Fast(_PREHASH_RegionInfo, _PREHASH_MaxAgents, agent_limit);
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_ObjectBonusFactor, object_bonus_factor);
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_BillableFactor, billable_factor);
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_WaterHeight, water_height);
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_TerrainRaiseLimit, terrain_raise_limit);
	msg->getF32Fast(_PREHASH_RegionInfo, _PREHASH_TerrainLowerLimit, terrain_lower_limit);
	msg->getS32Fast(_PREHASH_RegionInfo, _PREHASH_PricePerMeter, price_per_meter);
	msg->getS32Fast(_PREHASH_RegionInfo, _PREHASH_RedirectGridX, redirect_grid_x);
	msg->getS32Fast(_PREHASH_RegionInfo, _PREHASH_RedirectGridY, redirect_grid_y);

	// push values to the current LLViewerRegion
	LLViewerRegion *regionp = gAgent.getRegion();
	if (regionp)
	{
		regionp->setRegionNameAndZone(sim_name);
		regionp->setRegionFlags(region_flags);
		regionp->setSimAccess(sim_access);
		regionp->setWaterHeight(water_height);
		regionp->setBillableFactor(billable_factor);
	}
	
	LLFloaterGodTools* god_tools = LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools");
	if (!god_tools) return;

	// push values to god tools, if available
	if ( gAgent.isGodlike()
		&& LLFloaterReg::instanceVisible("god_tools")
		&& god_tools->mPanelRegionTools
		&& god_tools->mPanelObjectTools)
	{
		LLPanelRegionTools* rtool = god_tools->mPanelRegionTools;
		god_tools->mCurrentHost = host;

		// store locally
		rtool->setSimName(sim_name);
		rtool->setEstateID(estate_id);
		rtool->setParentEstateID(parent_estate_id);
		rtool->setCheckFlags(region_flags);
		rtool->setBillableFactor(billable_factor);
		rtool->setPricePerMeter(price_per_meter);
		rtool->setRedirectGridX(redirect_grid_x);
		rtool->setRedirectGridY(redirect_grid_y);
		rtool->enableAllWidgets();

		LLPanelObjectTools *otool = god_tools->mPanelObjectTools;
		otool->setCheckFlags(region_flags);
		otool->enableAllWidgets();

		LLViewerRegion *regionp = gAgent.getRegion();
		if ( !regionp )
		{
			// -1 implies non-existent
			rtool->setGridPosX(-1);
			rtool->setGridPosY(-1);
		}
		else
		{
			//compute the grid position of the region
			LLVector3d global_pos = regionp->getPosGlobalFromRegion(LLVector3::zero);
			S32 grid_pos_x = (S32) (global_pos.mdV[VX] / 256.0f);
			S32 grid_pos_y = (S32) (global_pos.mdV[VY] / 256.0f);

			rtool->setGridPosX(grid_pos_x);
			rtool->setGridPosY(grid_pos_y);
		}
	}
}


void LLFloaterGodTools::sendRegionInfoRequest()
{
	if (mPanelRegionTools) mPanelRegionTools->clearAllWidgets();
	if (mPanelObjectTools) mPanelObjectTools->clearAllWidgets();
	mCurrentHost = LLHost::invalid;
	mUpdateTimer.reset();

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("RequestRegionInfo");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	gAgent.sendReliableMessage();
}


void LLFloaterGodTools::sendGodUpdateRegionInfo()
{
	LLFloaterGodTools* god_tools = LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools");
	if (!god_tools) return;

	LLViewerRegion *regionp = gAgent.getRegion();
	if (gAgent.isGodlike()
		&& god_tools->mPanelRegionTools
		&& regionp
		&& gAgent.getRegionHost() == mCurrentHost)
	{
		LLMessageSystem *msg = gMessageSystem;
		LLPanelRegionTools *rtool = god_tools->mPanelRegionTools;

		msg->newMessage("GodUpdateRegionInfo");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_RegionInfo);
		msg->addStringFast(_PREHASH_SimName, rtool->getSimName());
		msg->addU32Fast(_PREHASH_EstateID, rtool->getEstateID());
		msg->addU32Fast(_PREHASH_ParentEstateID, rtool->getParentEstateID());
		msg->addU32Fast(_PREHASH_RegionFlags, computeRegionFlags());
		msg->addF32Fast(_PREHASH_BillableFactor, rtool->getBillableFactor());
		msg->addS32Fast(_PREHASH_PricePerMeter, rtool->getPricePerMeter());
		msg->addS32Fast(_PREHASH_RedirectGridX, rtool->getRedirectGridX());
		msg->addS32Fast(_PREHASH_RedirectGridY, rtool->getRedirectGridY());

		gAgent.sendReliableMessage();
	}
}

//*****************************************************************************
// LLPanelRegionTools
//*****************************************************************************


//   || Region |______________________________________
//   |                                                |
//   |  Sim Name: [________________________________]  |
//   |  ^         ^                                   |
//   |  LEFT      R1         Estate id:   [----]      |
//   |                       Parent id:   [----]      |
//   |  [X] Prelude          Grid Pos:     [--] [--]  |
//   |  [X] Visible          Redirect Pos: [--] [--]  |
//   |  [X] Damage           Bill Factor  [8_______]  |
//   |  [X] Block Terraform  PricePerMeter[8_______]  |
//   |                                    [Apply]     |
//   |                                                |
//   |  [Bake Terrain]            [Select Region]     |
//   |  [Revert Terrain]          [Autosave Now]      |
//   |  [Swap Terrain]                                | 
//   |				                                  | 
//   |________________________________________________|
//      ^                    ^                     ^
//      LEFT                 R2                   RIGHT


// Floats because spinners only support floats. JC
const F32 BILLABLE_FACTOR_DEFAULT = 1;
const F32 BILLABLE_FACTOR_MIN = 0.0f;
const F32 BILLABLE_FACTOR_MAX = 4.f;

// floats because spinners only understand floats. JC
const F32 PRICE_PER_METER_DEFAULT = 1.f;
const F32 PRICE_PER_METER_MIN = 0.f;
const F32 PRICE_PER_METER_MAX = 100.f;


LLPanelRegionTools::LLPanelRegionTools()
: 	LLPanel()
{
	mCommitCallbackRegistrar.add("RegionTools.ChangeAnything",	boost::bind(&LLPanelRegionTools::onChangeAnything, this));
	mCommitCallbackRegistrar.add("RegionTools.ChangePrelude",	boost::bind(&LLPanelRegionTools::onChangePrelude, this));
	mCommitCallbackRegistrar.add("RegionTools.BakeTerrain",		boost::bind(&LLPanelRegionTools::onBakeTerrain, this));
	mCommitCallbackRegistrar.add("RegionTools.RevertTerrain",	boost::bind(&LLPanelRegionTools::onRevertTerrain, this));	
	mCommitCallbackRegistrar.add("RegionTools.SwapTerrain",		boost::bind(&LLPanelRegionTools::onSwapTerrain, this));		
	mCommitCallbackRegistrar.add("RegionTools.Refresh",			boost::bind(&LLPanelRegionTools::onRefresh, this));		
	mCommitCallbackRegistrar.add("RegionTools.ApplyChanges",	boost::bind(&LLPanelRegionTools::onApplyChanges, this));	
	mCommitCallbackRegistrar.add("RegionTools.SelectRegion",	boost::bind(&LLPanelRegionTools::onSelectRegion, this));
	mCommitCallbackRegistrar.add("RegionTools.SaveState",		boost::bind(&LLPanelRegionTools::onSaveState, this));	
}

BOOL LLPanelRegionTools::postBuild()
{
	getChild<LLLineEditor>("region name")->setKeystrokeCallback(onChangeSimName, this);
	childSetPrevalidate("region name", &LLTextValidate::validateASCIIPrintableNoPipe);
	childSetPrevalidate("estate", &LLTextValidate::validatePositiveS32);
	childSetPrevalidate("parentestate", &LLTextValidate::validatePositiveS32);
	childDisable("parentestate");
	childSetPrevalidate("gridposx", &LLTextValidate::validatePositiveS32);
	childDisable("gridposx");
	childSetPrevalidate("gridposy", &LLTextValidate::validatePositiveS32);
	childDisable("gridposy");
	
	childSetPrevalidate("redirectx", &LLTextValidate::validatePositiveS32);
	childSetPrevalidate("redirecty", &LLTextValidate::validatePositiveS32);
			 
	return TRUE;
}

// Destroys the object
LLPanelRegionTools::~LLPanelRegionTools()
{
	// base class will take care of everything
}

U32 LLPanelRegionTools::computeRegionFlags(U32 flags) const
{
	flags &= getRegionFlagsMask();
	flags |= getRegionFlags();
	return flags;
}


void LLPanelRegionTools::refresh()
{
}


void LLPanelRegionTools::clearAllWidgets()
{
	// clear all widgets
	childSetValue("region name", "unknown");
	childSetFocus("region name", FALSE);

	childSetValue("check prelude", FALSE);
	childDisable("check prelude");

	childSetValue("check fixed sun", FALSE);
	childDisable("check fixed sun");

	childSetValue("check reset home", FALSE);
	childDisable("check reset home");

	childSetValue("check damage", FALSE);
	childDisable("check damage");

	childSetValue("check visible", FALSE);
	childDisable("check visible");

	childSetValue("block terraform", FALSE);
	childDisable("block terraform");

	childSetValue("block dwell", FALSE);
	childDisable("block dwell");

	childSetValue("is sandbox", FALSE);
	childDisable("is sandbox");

	childSetValue("billable factor", BILLABLE_FACTOR_DEFAULT);
	childDisable("billable factor");

	childSetValue("land cost", PRICE_PER_METER_DEFAULT);
	childDisable("land cost");

	childDisable("Apply");
	childDisable("Bake Terrain");
	childDisable("Autosave now");
}


void LLPanelRegionTools::enableAllWidgets()
{
	// enable all of the widgets
	
	childEnable("check prelude");
	childEnable("check fixed sun");
	childEnable("check reset home");
	childEnable("check damage");
	childDisable("check visible"); // use estates to update...
	childEnable("block terraform");
	childEnable("block dwell");
	childEnable("is sandbox");
	
	childEnable("billable factor");
	childEnable("land cost");

	childDisable("Apply");	// don't enable this one
	childEnable("Bake Terrain");
	childEnable("Autosave now");
}

void LLPanelRegionTools::onSaveState(void* userdata)
{
	if (gAgent.isGodlike())
	{
		// Send message to save world state
		gMessageSystem->newMessageFast(_PREHASH_StateSave);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_DataBlock);
		gMessageSystem->addStringFast(_PREHASH_Filename, NULL);
		gAgent.sendReliableMessage();
	}
}

const std::string LLPanelRegionTools::getSimName() const
{
	return childGetValue("region name");
}

U32 LLPanelRegionTools::getEstateID() const
{
	U32 id = (U32)childGetValue("estate").asInteger();
	return id;
}

U32 LLPanelRegionTools::getParentEstateID() const
{
	U32 id = (U32)childGetValue("parentestate").asInteger();
	return id;
}

S32 LLPanelRegionTools::getRedirectGridX() const
{
	return childGetValue("redirectx").asInteger();
}

S32 LLPanelRegionTools::getRedirectGridY() const
{
	return childGetValue("redirecty").asInteger();
}

S32 LLPanelRegionTools::getGridPosX() const
{
	return childGetValue("gridposx").asInteger();
}

S32 LLPanelRegionTools::getGridPosY() const
{
	return childGetValue("gridposy").asInteger();
}

U32 LLPanelRegionTools::getRegionFlags() const
{
	U32 flags = 0x0;
	flags = childGetValue("check prelude").asBoolean()  
					? set_prelude_flags(flags)
					: unset_prelude_flags(flags);

	// override prelude
	if (childGetValue("check fixed sun").asBoolean())
	{
		flags |= REGION_FLAGS_SUN_FIXED;
	}
	if (childGetValue("check reset home").asBoolean())
	{
		flags |= REGION_FLAGS_RESET_HOME_ON_TELEPORT;
	}
	if (childGetValue("check visible").asBoolean())
	{
		flags |= REGION_FLAGS_EXTERNALLY_VISIBLE;
	}
	if (childGetValue("check damage").asBoolean())
	{
		flags |= REGION_FLAGS_ALLOW_DAMAGE;
	}
	if (childGetValue("block terraform").asBoolean())
	{
		flags |= REGION_FLAGS_BLOCK_TERRAFORM;
	}
	if (childGetValue("block dwell").asBoolean())
	{
		flags |= REGION_FLAGS_BLOCK_DWELL;
	}
	if (childGetValue("is sandbox").asBoolean())
	{
		flags |= REGION_FLAGS_SANDBOX;
	}
	return flags;
}

U32 LLPanelRegionTools::getRegionFlagsMask() const
{
	U32 flags = 0xffffffff;
	flags = childGetValue("check prelude").asBoolean()
				? set_prelude_flags(flags)
				: unset_prelude_flags(flags);

	if (!childGetValue("check fixed sun").asBoolean())
	{
		flags &= ~REGION_FLAGS_SUN_FIXED;
	}
	if (!childGetValue("check reset home").asBoolean())
	{
		flags &= ~REGION_FLAGS_RESET_HOME_ON_TELEPORT;
	}
	if (!childGetValue("check visible").asBoolean())
	{
		flags &= ~REGION_FLAGS_EXTERNALLY_VISIBLE;
	}
	if (!childGetValue("check damage").asBoolean())
	{
		flags &= ~REGION_FLAGS_ALLOW_DAMAGE;
	}
	if (!childGetValue("block terraform").asBoolean())
	{
		flags &= ~REGION_FLAGS_BLOCK_TERRAFORM;
	}
	if (!childGetValue("block dwell").asBoolean())
	{
		flags &= ~REGION_FLAGS_BLOCK_DWELL;
	}
	if (!childGetValue("is sandbox").asBoolean())
	{
		flags &= ~REGION_FLAGS_SANDBOX;
	}
	return flags;
}

F32 LLPanelRegionTools::getBillableFactor() const
{
	return (F32)childGetValue("billable factor").asReal();
}

S32 LLPanelRegionTools::getPricePerMeter() const
{
	return childGetValue("land cost");
}

void LLPanelRegionTools::setSimName(const std::string& name)
{
	childSetValue("region name", name);
}

void LLPanelRegionTools::setEstateID(U32 id)
{
	childSetValue("estate", (S32)id);
}

void LLPanelRegionTools::setGridPosX(S32 pos)
{
	childSetValue("gridposx", pos);
}

void LLPanelRegionTools::setGridPosY(S32 pos)
{
	childSetValue("gridposy", pos);
}

void LLPanelRegionTools::setRedirectGridX(S32 pos)
{
	childSetValue("redirectx", pos);
}

void LLPanelRegionTools::setRedirectGridY(S32 pos)
{
	childSetValue("redirecty", pos);
}

void LLPanelRegionTools::setParentEstateID(U32 id)
{
	childSetValue("parentestate", (S32)id);
}

void LLPanelRegionTools::setCheckFlags(U32 flags)
{
	childSetValue("check prelude", is_prelude(flags) ? TRUE : FALSE);
	childSetValue("check fixed sun", flags & REGION_FLAGS_SUN_FIXED ? TRUE : FALSE);
	childSetValue("check reset home", flags & REGION_FLAGS_RESET_HOME_ON_TELEPORT ? TRUE : FALSE);
	childSetValue("check damage", flags & REGION_FLAGS_ALLOW_DAMAGE ? TRUE : FALSE);
	childSetValue("check visible", flags & REGION_FLAGS_EXTERNALLY_VISIBLE ? TRUE : FALSE);
	childSetValue("block terraform", flags & REGION_FLAGS_BLOCK_TERRAFORM ? TRUE : FALSE);
	childSetValue("block dwell", flags & REGION_FLAGS_BLOCK_DWELL ? TRUE : FALSE);
	childSetValue("is sandbox", flags & REGION_FLAGS_SANDBOX ? TRUE : FALSE );
}

void LLPanelRegionTools::setBillableFactor(F32 billable_factor)
{
	childSetValue("billable factor", billable_factor);
}

void LLPanelRegionTools::setPricePerMeter(S32 price)
{
	childSetValue("land cost", price);
}

void LLPanelRegionTools::onChangeAnything()
{
	if (gAgent.isGodlike())
	{
		childEnable("Apply");
	}
}

void LLPanelRegionTools::onChangePrelude()
{
	// checking prelude auto-checks fixed sun
	if (childGetValue("check prelude").asBoolean())
	{
		childSetValue("check fixed sun", TRUE);
		childSetValue("check reset home", TRUE);
		onChangeAnything();
	}
	// pass on to default onChange handler

}

// static
void LLPanelRegionTools::onChangeSimName(LLLineEditor* caller, void* userdata )
{
	if (userdata && gAgent.isGodlike())
	{
		LLPanelRegionTools* region_tools = (LLPanelRegionTools*) userdata;
		region_tools->childEnable("Apply");
	}
}


void LLPanelRegionTools::onRefresh()
{
	LLFloaterGodTools* god_tools = LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools");
	if(!god_tools) return;
	LLViewerRegion *region = gAgent.getRegion();
	if (region && gAgent.isGodlike())
	{
		god_tools->sendRegionInfoRequest();
		//LLFloaterGodTools::getInstance()->sendRegionInfoRequest();
		//LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools")->sendRegionInfoRequest();
	}
}

void LLPanelRegionTools::onApplyChanges()
{
	LLFloaterGodTools* god_tools = LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools");
	if(!god_tools) return;
	LLViewerRegion *region = gAgent.getRegion();
	if (region && gAgent.isGodlike())
	{
		childDisable("Apply");
		god_tools->sendGodUpdateRegionInfo();
		//LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools")->sendGodUpdateRegionInfo();
	}
}

void LLPanelRegionTools::onBakeTerrain()
{
	LLPanelRequestTools::sendRequest("terrain", "bake", gAgent.getRegionHost());
}

void LLPanelRegionTools::onRevertTerrain()
{
	LLPanelRequestTools::sendRequest("terrain", "revert", gAgent.getRegionHost());
}


void LLPanelRegionTools::onSwapTerrain()
{
	LLPanelRequestTools::sendRequest("terrain", "swap", gAgent.getRegionHost());
}

void LLPanelRegionTools::onSelectRegion()
{
	llinfos << "LLPanelRegionTools::onSelectRegion" << llendl;

	LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromPosGlobal(gAgent.getPositionGlobal());
	if (!regionp)
	{
		return;
	}

	LLVector3d north_east(REGION_WIDTH_METERS, REGION_WIDTH_METERS, 0);
	LLViewerParcelMgr::getInstance()->selectLand(regionp->getOriginGlobal(), 
						   regionp->getOriginGlobal() + north_east, FALSE);
	
}


//*****************************************************************************
// Class LLPanelGridTools
//*****************************************************************************

//   || Grid   |_____________________________________
//   |                                               |
//   |                                               |
//   |  Sun Phase: >--------[]---------< [________]  |
//   |                                               |
//   |  ^         ^                                  |
//   |  LEFT      R1                                 |
//   |                                               |
//   |  [Kick all users]                             | 
//   |                                               |
//   |                                               |
//   |                                               |
//   |                                               |
//   |                                               |
//   |_______________________________________________|
//      ^                                ^        ^
//      LEFT                             R2       RIGHT

const F32 HOURS_TO_RADIANS = (2.f*F_PI)/24.f;


LLPanelGridTools::LLPanelGridTools() :
	LLPanel()
{
	mCommitCallbackRegistrar.add("GridTools.FlushMapVisibilityCaches",		boost::bind(&LLPanelGridTools::onClickFlushMapVisibilityCaches, this));	
}

// Destroys the object
LLPanelGridTools::~LLPanelGridTools()
{
}

BOOL LLPanelGridTools::postBuild()
{
	return TRUE;
}

void LLPanelGridTools::refresh()
{
}

void LLPanelGridTools::onClickFlushMapVisibilityCaches()
{
	LLNotificationsUtil::add("FlushMapVisibilityCaches", LLSD(), LLSD(), flushMapVisibilityCachesConfirm);
}

// static
bool LLPanelGridTools::flushMapVisibilityCachesConfirm(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option != 0) return false;

	// HACK: Send this as an EstateOwnerRequest so it gets routed
	// correctly by the spaceserver. JC
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("EstateOwnerMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", "refreshmapvisibility");
	msg->addUUID("Invoice", LLUUID::null);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", gAgent.getID().asString());
	gAgent.sendReliableMessage();
	return false;
}


//*****************************************************************************
// LLPanelObjectTools
//*****************************************************************************


//   || Object |_______________________________________________________
//   |                                                                 |
//   |  Sim Name: Foo                                                  |
//   |  ^         ^                                                    |
//   |  LEFT      R1                                                   |
//   |                                                                 |
//   |  [X] Disable Scripts [X] Disable Collisions [X] Disable Physics |
//   |                                                  [ Apply  ]     |
//   |                                                                 |
//   |  [Set Target Avatar]	Avatar Name                                |
//   |  [Delete Target's Objects on Public Land	   ]                   |
//   |  [Delete All Target's Objects			   ]                   |
//   |  [Delete All Scripted Objects on Public Land]                   |
//   |  [Get Top Colliders ]                                           |
//   |  [Get Top Scripts   ]                                           |
//   |_________________________________________________________________|
//      ^                                         ^
//      LEFT                                      RIGHT

// Default constructor
LLPanelObjectTools::LLPanelObjectTools() 
	: 	LLPanel(),
		mTargetAvatar()
{
	mCommitCallbackRegistrar.add("ObjectTools.ChangeAnything",		boost::bind(&LLPanelObjectTools::onChangeAnything, this));
	mCommitCallbackRegistrar.add("ObjectTools.DeletePublicOwnedBy",	boost::bind(&LLPanelObjectTools::onClickDeletePublicOwnedBy, this));
	mCommitCallbackRegistrar.add("ObjectTools.DeleteAllScriptedOwnedBy",		boost::bind(&LLPanelObjectTools::onClickDeleteAllScriptedOwnedBy, this));
	mCommitCallbackRegistrar.add("ObjectTools.DeleteAllOwnedBy",		boost::bind(&LLPanelObjectTools::onClickDeleteAllOwnedBy, this));
	mCommitCallbackRegistrar.add("ObjectTools.ApplyChanges",		boost::bind(&LLPanelObjectTools::onApplyChanges, this));
	mCommitCallbackRegistrar.add("ObjectTools.Set",		boost::bind(&LLPanelObjectTools::onClickSet, this));
	mCommitCallbackRegistrar.add("ObjectTools.GetTopColliders",		boost::bind(&LLPanelObjectTools::onGetTopColliders, this));
	mCommitCallbackRegistrar.add("ObjectTools.GetTopScripts",		boost::bind(&LLPanelObjectTools::onGetTopScripts, this));
	mCommitCallbackRegistrar.add("ObjectTools.GetScriptDigest",		boost::bind(&LLPanelObjectTools::onGetScriptDigest, this));
}

// Destroys the object
LLPanelObjectTools::~LLPanelObjectTools()
{
	// base class will take care of everything
}

BOOL LLPanelObjectTools::postBuild()
{
	refresh();
	return TRUE;
}

void LLPanelObjectTools::setTargetAvatar(const LLUUID &target_id)
{
	mTargetAvatar = target_id;
	if (target_id.isNull())
	{
		childSetValue("target_avatar_name", getString("no_target"));
	}
} 


void LLPanelObjectTools::refresh()
{
	LLViewerRegion *regionp = gAgent.getRegion();
	if (regionp)
	{
		childSetText("region name", regionp->getName());
	}
}


U32 LLPanelObjectTools::computeRegionFlags(U32 flags) const
{
	if (childGetValue("disable scripts").asBoolean())
	{
		flags |= REGION_FLAGS_SKIP_SCRIPTS;
	}
	else
	{
		flags &= ~REGION_FLAGS_SKIP_SCRIPTS;
	}
	if (childGetValue("disable collisions").asBoolean())
	{
		flags |= REGION_FLAGS_SKIP_COLLISIONS;
	}
	else
	{
		flags &= ~REGION_FLAGS_SKIP_COLLISIONS;
	}
	if (childGetValue("disable physics").asBoolean())
	{
		flags |= REGION_FLAGS_SKIP_PHYSICS;
	}
	else
	{
		flags &= ~REGION_FLAGS_SKIP_PHYSICS;
	}
	return flags;
}


void LLPanelObjectTools::setCheckFlags(U32 flags)
{
	childSetValue("disable scripts", flags & REGION_FLAGS_SKIP_SCRIPTS ? TRUE : FALSE);
	childSetValue("disable collisions", flags & REGION_FLAGS_SKIP_COLLISIONS ? TRUE : FALSE);
	childSetValue("disable physics", flags & REGION_FLAGS_SKIP_PHYSICS ? TRUE : FALSE);
}


void LLPanelObjectTools::clearAllWidgets()
{
	childSetValue("disable scripts", FALSE);
	childDisable("disable scripts");

	childDisable("Apply");
	childDisable("Set Target");
	childDisable("Delete Target's Scripted Objects On Others Land");
	childDisable("Delete Target's Scripted Objects On *Any* Land");
	childDisable("Delete *ALL* Of Target's Objects");
}


void LLPanelObjectTools::enableAllWidgets()
{
	childEnable("disable scripts");

	childDisable("Apply");	// don't enable this one
	childEnable("Set Target");
	childEnable("Delete Target's Scripted Objects On Others Land");
	childEnable("Delete Target's Scripted Objects On *Any* Land");
	childEnable("Delete *ALL* Of Target's Objects");
	childEnable("Get Top Colliders");
	childEnable("Get Top Scripts");
}


void LLPanelObjectTools::onGetTopColliders()
{
	LLFloaterTopObjects* instance = LLFloaterReg::getTypedInstance<LLFloaterTopObjects>("top_objects");
	if(!instance) return;
	
	if (gAgent.isGodlike())
	{
		LLFloaterReg::showInstance("top_objects");
		LLFloaterTopObjects::setMode(STAT_REPORT_TOP_COLLIDERS);
		instance->onRefresh();
	}
}

void LLPanelObjectTools::onGetTopScripts()
{
	LLFloaterTopObjects* instance = LLFloaterReg::getTypedInstance<LLFloaterTopObjects>("top_objects");
	if(!instance) return;
	
	if (gAgent.isGodlike()) 
	{
		LLFloaterReg::showInstance("top_objects");
		LLFloaterTopObjects::setMode(STAT_REPORT_TOP_SCRIPTS);
		instance->onRefresh();
	}
}

void LLPanelObjectTools::onGetScriptDigest()
{
	if (gAgent.isGodlike())
	{
		// get the list of scripts and number of occurences of each
		// (useful for finding self-replicating objects)
		LLPanelRequestTools::sendRequest("scriptdigest","0",gAgent.getRegionHost());
	}
}

void LLPanelObjectTools::onClickDeletePublicOwnedBy()
{
	// Bring up view-modal dialog

	if (!mTargetAvatar.isNull())
	{
		mSimWideDeletesFlags = 
			SWD_SCRIPTED_ONLY | SWD_OTHERS_LAND_ONLY;

		LLSD args;
		args["AVATAR_NAME"] = childGetValue("target_avatar_name").asString();
		LLSD payload;
		payload["avatar_id"] = mTargetAvatar;
		payload["flags"] = (S32)mSimWideDeletesFlags;

		LLNotificationsUtil::add( "GodDeleteAllScriptedPublicObjectsByUser",
								args,
								payload,
								callbackSimWideDeletes);
	}
}

void LLPanelObjectTools::onClickDeleteAllScriptedOwnedBy()
{
	// Bring up view-modal dialog
	if (!mTargetAvatar.isNull())
	{
		mSimWideDeletesFlags = SWD_SCRIPTED_ONLY;

		LLSD args;
		args["AVATAR_NAME"] = childGetValue("target_avatar_name").asString();
		LLSD payload;
		payload["avatar_id"] = mTargetAvatar;
		payload["flags"] = (S32)mSimWideDeletesFlags;

		LLNotificationsUtil::add( "GodDeleteAllScriptedObjectsByUser",
								args,
								payload,
								callbackSimWideDeletes);
	}
}

void LLPanelObjectTools::onClickDeleteAllOwnedBy()
{
	// Bring up view-modal dialog
	if (!mTargetAvatar.isNull())
	{
		mSimWideDeletesFlags = 0;

		LLSD args;
		args["AVATAR_NAME"] = childGetValue("target_avatar_name").asString();
		LLSD payload;
		payload["avatar_id"] = mTargetAvatar;
		payload["flags"] = (S32)mSimWideDeletesFlags;

		LLNotificationsUtil::add( "GodDeleteAllObjectsByUser",
								args,
								payload,
								callbackSimWideDeletes);
	}
}

// static
bool LLPanelObjectTools::callbackSimWideDeletes( const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		if (!notification["payload"]["avatar_id"].asUUID().isNull())
		{
			send_sim_wide_deletes(notification["payload"]["avatar_id"].asUUID(), 
								  notification["payload"]["flags"].asInteger());
		}
	}
	return false;
}

void LLPanelObjectTools::onClickSet()
{
	// grandparent is a floater, which can have a dependent
	gFloaterView->getParentFloater(this)->addDependentFloater(LLFloaterAvatarPicker::show(boost::bind(&LLPanelObjectTools::callbackAvatarID, this, _1,_2)));
}

void LLPanelObjectTools::onClickSetBySelection(void* data)
{
	LLPanelObjectTools* panelp = (LLPanelObjectTools*) data;
	if (!panelp) return;

	const BOOL non_root_ok = TRUE; 
	LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode(NULL, non_root_ok);
	if (!node) return;

	std::string owner_name;
	LLUUID owner_id;
	LLSelectMgr::getInstance()->selectGetOwner(owner_id, owner_name);

	panelp->mTargetAvatar = owner_id;
	LLStringUtil::format_map_t args;
	args["[OBJECT]"] = node->mName;
	args["[OWNER]"] = owner_name;
	std::string name = LLTrans::getString("GodToolsObjectOwnedBy", args);
	panelp->childSetValue("target_avatar_name", name);
}

void LLPanelObjectTools::callbackAvatarID(const std::vector<std::string>& names, const uuid_vec_t& ids)
{
	if (ids.empty() || names.empty()) return;
	mTargetAvatar = ids[0];
	childSetValue("target_avatar_name", names[0]);
	refresh();
}

void LLPanelObjectTools::onChangeAnything()
{
	if (gAgent.isGodlike())
	{
		childEnable("Apply");
	}
}

void LLPanelObjectTools::onApplyChanges()
{
	LLFloaterGodTools* god_tools = LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools");
	if(!god_tools) return;
	LLViewerRegion *region = gAgent.getRegion();
	if (region && gAgent.isGodlike())
	{
		// TODO -- implement this
		childDisable("Apply");
		god_tools->sendGodUpdateRegionInfo();
		//LLFloaterReg::getTypedInstance<LLFloaterGodTools>("god_tools")->sendGodUpdateRegionInfo();
	}
}


// --------------------
// LLPanelRequestTools
// --------------------

const std::string SELECTION = "Selection";
const std::string AGENT_REGION = "Agent Region";

LLPanelRequestTools::LLPanelRequestTools():
	LLPanel()
{
	mCommitCallbackRegistrar.add("GodTools.Request",		boost::bind(&LLPanelRequestTools::onClickRequest, this));
}

LLPanelRequestTools::~LLPanelRequestTools()
{
}

BOOL LLPanelRequestTools::postBuild()
{
	refresh();

	return TRUE;
}

void LLPanelRequestTools::refresh()
{
	std::string buffer = childGetValue("destination");
	LLCtrlListInterface *list = childGetListInterface("destination");
	if (!list) return;

	S32 last_item = list->getItemCount();

	if (last_item >=3)
	{
	list->selectItemRange(2,last_item);
	list->operateOnSelection(LLCtrlListInterface::OP_DELETE);
	}
	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
		 iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		std::string name = regionp->getName();
		if (!name.empty())
		{
			list->addSimpleElement(name);
		}
	}
	if(!buffer.empty())
	{
		list->selectByValue(buffer);
	}
	else
	{
		list->operateOnSelection(LLCtrlListInterface::OP_DESELECT);
	}
}


// static
void LLPanelRequestTools::sendRequest(const std::string& request, 
									  const std::string& parameter, 
									  const LLHost& host)
{
	llinfos << "Sending request '" << request << "', '"
			<< parameter << "' to " << host << llendl;
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("GodlikeMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", request);
	msg->addUUID("Invoice", LLUUID::null);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", parameter);
	msg->sendReliable(host);
}

void LLPanelRequestTools::onClickRequest()
{
	const std::string dest = childGetValue("destination").asString();
	if(dest == SELECTION)
	{
		std::string req =childGetValue("request");
		req = req.substr(0, req.find_first_of(" "));
		std::string param = childGetValue("parameter");
		LLSelectMgr::getInstance()->sendGodlikeRequest(req, param);
	}
	else if(dest == AGENT_REGION)
	{
		sendRequest(gAgent.getRegionHost());
	}
	else
	{
		// find region by name
		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
			 iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* regionp = *iter;
			if(dest == regionp->getName())
			{
				// found it
				sendRequest(regionp->getHost());
			}
		}
	}
}

void terrain_download_done(void** data, S32 status, LLExtStat ext_status)
{
	LLNotificationsUtil::add("TerrainDownloaded");
}


void test_callback(const LLTSCode status)
{
	llinfos << "Test transfer callback returned!" << llendl;
}


void LLPanelRequestTools::sendRequest(const LLHost& host)
{

	// intercept viewer local actions here
	std::string req = childGetValue("request");
	if (req == "terrain download")
	{
		gXferManager->requestFile(std::string("terrain.raw"), std::string("terrain.raw"), LL_PATH_NONE,
								  host,
								  FALSE,
								  terrain_download_done,
								  NULL);
	}
	else
	{
		req = req.substr(0, req.find_first_of(" "));
		sendRequest(req, childGetValue("parameter").asString(), host);
	}
}

// Flags are SWD_ flags.
void send_sim_wide_deletes(const LLUUID& owner_id, U32 flags)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_SimWideDeletes);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_DataBlock);
	msg->addUUIDFast(_PREHASH_TargetID, owner_id);
	msg->addU32Fast(_PREHASH_Flags, flags);
	gAgent.sendReliableMessage();
}
