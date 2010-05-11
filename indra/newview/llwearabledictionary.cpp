/** 
 * @file llwearableDictionary.cpp
 * @brief LLWearableDictionary class implementation
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
#include "llwearabledictionary.h"
#include "lltrans.h"

LLWearableDictionary::LLWearableDictionary()
{
}

LLWearableDictionary::~LLWearableDictionary()
{
}

LLWearableDictionary::Wearables::Wearables()
{
	addEntry(WT_SHAPE,        new WearableEntry("shape",       "New Shape",		LLAssetType::AT_BODYPART));
	addEntry(WT_SKIN,         new WearableEntry("skin",        "New Skin",			LLAssetType::AT_BODYPART));
	addEntry(WT_HAIR,         new WearableEntry("hair",        "New Hair",			LLAssetType::AT_BODYPART));
	addEntry(WT_EYES,         new WearableEntry("eyes",        "New Eyes",			LLAssetType::AT_BODYPART));
	addEntry(WT_SHIRT,        new WearableEntry("shirt",       "New Shirt",		LLAssetType::AT_CLOTHING));
	addEntry(WT_PANTS,        new WearableEntry("pants",       "New Pants",		LLAssetType::AT_CLOTHING));
	addEntry(WT_SHOES,        new WearableEntry("shoes",       "New Shoes",		LLAssetType::AT_CLOTHING));
	addEntry(WT_SOCKS,        new WearableEntry("socks",       "New Socks",		LLAssetType::AT_CLOTHING));
	addEntry(WT_JACKET,       new WearableEntry("jacket",      "New Jacket",		LLAssetType::AT_CLOTHING));
	addEntry(WT_GLOVES,       new WearableEntry("gloves",      "New Gloves",		LLAssetType::AT_CLOTHING));
	addEntry(WT_UNDERSHIRT,   new WearableEntry("undershirt",  "New Undershirt",	LLAssetType::AT_CLOTHING));
	addEntry(WT_UNDERPANTS,   new WearableEntry("underpants",  "New Underpants",	LLAssetType::AT_CLOTHING));
	addEntry(WT_SKIRT,        new WearableEntry("skirt",       "New Skirt",		LLAssetType::AT_CLOTHING));
	addEntry(WT_ALPHA,        new WearableEntry("alpha",       "New Alpha",		LLAssetType::AT_CLOTHING));
	addEntry(WT_TATTOO,       new WearableEntry("tattoo",      "New Tattoo",		LLAssetType::AT_CLOTHING));
	addEntry(WT_INVALID,      new WearableEntry("invalid",     "Invalid Wearable", LLAssetType::AT_NONE));
	addEntry(WT_COUNT,        NULL);
}

LLWearableDictionary::WearableEntry::WearableEntry(const std::string &name,
												   const std::string& default_new_name,
												   LLAssetType::EType assetType) :
	LLDictionaryEntry(name),
	mAssetType(assetType),
	mDefaultNewName(default_new_name),
	mLabel(LLTrans::getString(name))
{
}

// static
EWearableType LLWearableDictionary::typeNameToType(const std::string& type_name)
{
	const EWearableType wearable = getInstance()->getWearableEntry(type_name);
	return wearable;
}

// static 
const std::string& LLWearableDictionary::getTypeName(EWearableType type)
{ 
	return getInstance()->getWearableEntry(type)->mName;
}

//static 
const std::string& LLWearableDictionary::getTypeDefaultNewName(EWearableType type)
{ 
	return getInstance()->getWearableEntry(type)->mDefaultNewName;
}

// static 
const std::string& LLWearableDictionary::getTypeLabel(EWearableType type)
{ 
	return getInstance()->getWearableEntry(type)->mLabel;
}

// static 
LLAssetType::EType LLWearableDictionary::getAssetType(EWearableType type)
{
	return getInstance()->getWearableEntry(type)->mAssetType;
}

