/** 
 * @file llpaneleditwearable.cpp
 * @brief UI panel for editing of a particular wearable item.
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

#include "llpaneleditwearable.h"
#include "llpanel.h"
#include "llwearable.h"
#include "lluictrl.h"
#include "llscrollingpanellist.h"
#include "llvisualparam.h"
#include "lltoolmorph.h"
#include "llviewerjointmesh.h"
#include "lltrans.h"
#include "llbutton.h"
#include "llsliderctrl.h"
#include "llagent.h"
#include "llvoavatarself.h"
#include "lltexteditor.h"
#include "lltextbox.h"
#include "llaccordionctrltab.h"
#include "llagentwearables.h"
#include "llscrollingpanelparam.h"

#include "llcolorswatch.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "llviewercontrol.h"	// gSavedSettings
#include "llviewertexturelist.h"

// register panel with appropriate XML
static LLRegisterPanelClassWrapper<LLPanelEditWearable> t_edit_wearable("panel_edit_wearable");

// subparts of the UI for focus, camera position, etc.
enum ESubpart {
	SUBPART_SHAPE_HEAD = 1, // avoid 0
	SUBPART_SHAPE_EYES,
	SUBPART_SHAPE_EARS,
	SUBPART_SHAPE_NOSE,
	SUBPART_SHAPE_MOUTH,
	SUBPART_SHAPE_CHIN,
	SUBPART_SHAPE_TORSO,
	SUBPART_SHAPE_LEGS,
	SUBPART_SHAPE_WHOLE,
	SUBPART_SHAPE_DETAIL,
	SUBPART_SKIN_COLOR,
	SUBPART_SKIN_FACEDETAIL,
	SUBPART_SKIN_MAKEUP,
	SUBPART_SKIN_BODYDETAIL,
	SUBPART_HAIR_COLOR,
	SUBPART_HAIR_STYLE,
	SUBPART_HAIR_EYEBROWS,
	SUBPART_HAIR_FACIAL,
	SUBPART_EYES,
	SUBPART_SHIRT,
	SUBPART_PANTS,
	SUBPART_SHOES,
	SUBPART_SOCKS,
	SUBPART_JACKET,
	SUBPART_GLOVES,
	SUBPART_UNDERSHIRT,
	SUBPART_UNDERPANTS,
	SUBPART_SKIRT,
	SUBPART_ALPHA,
	SUBPART_TATTOO
 };

using namespace LLVOAvatarDefines;

typedef std::vector<ESubpart> subpart_vec_t;

// Locally defined classes

class LLEditWearableDictionary : public LLSingleton<LLEditWearableDictionary>
{
	//--------------------------------------------------------------------
	// Constructors and Destructors
	//--------------------------------------------------------------------
public:
	LLEditWearableDictionary();
	virtual ~LLEditWearableDictionary();
	
	//--------------------------------------------------------------------
	// Wearable Types
	//--------------------------------------------------------------------
public:
	struct WearableEntry : public LLDictionaryEntry
	{
		WearableEntry(EWearableType type,
					  const std::string &title,
					  const std::string &desc_title,
					  U8 num_color_swatches,  // number of 'color_swatches'
					  U8 num_texture_pickers, // number of 'texture_pickers'
					  U8 num_subparts, ... ); // number of subparts followed by a list of ETextureIndex and ESubparts


		const EWearableType mWearableType;
		const std::string   mTitle;
		const std::string	mDescTitle;
		subpart_vec_t		mSubparts;
		texture_vec_t		mColorSwatchCtrls;
		texture_vec_t		mTextureCtrls;
	};

	struct Wearables : public LLDictionary<EWearableType, WearableEntry>
	{
		Wearables();
	} mWearables;

	const WearableEntry*	getWearable(EWearableType type) const { return mWearables.lookup(type); }

	//--------------------------------------------------------------------
	// Subparts
	//--------------------------------------------------------------------
public:
	struct SubpartEntry : public LLDictionaryEntry
	{
		SubpartEntry(ESubpart part,
					 const std::string &joint,
					 const std::string &edit_group,
					 const std::string &param_list,
					 const std::string &accordion_tab,
					 const LLVector3d  &target_offset,
					 const LLVector3d  &camera_offset,
					 const ESex 	   &sex);

		ESubpart			mSubpart;
		std::string			mTargetJoint;
		std::string			mEditGroup;
		std::string			mParamList;
		std::string			mAccordionTab;
		LLVector3d			mTargetOffset;
		LLVector3d			mCameraOffset;
		ESex				mSex;
	};

	struct Subparts : public LLDictionary<ESubpart, SubpartEntry>
	{
		Subparts();
	} mSubparts;

	const SubpartEntry*  getSubpart(ESubpart subpart) const { return mSubparts.lookup(subpart); }

	//--------------------------------------------------------------------
	// Picker Control Entries
	//--------------------------------------------------------------------
public:
	struct PickerControlEntry : public LLDictionaryEntry
	{
		PickerControlEntry(ETextureIndex tex_index,
						   const std::string name,
						   const LLUUID default_image_id = LLUUID::null,
						   const bool allow_no_texture = false);
		ETextureIndex		mTextureIndex;
		const std::string	mControlName;
		const LLUUID		mDefaultImageId;
		const bool			mAllowNoTexture;
	};

	struct ColorSwatchCtrls : public LLDictionary<ETextureIndex, PickerControlEntry>
	{
		ColorSwatchCtrls();
	} mColorSwatchCtrls;

	struct TextureCtrls : public LLDictionary<ETextureIndex, PickerControlEntry>
	{
		TextureCtrls();
	} mTextureCtrls;

	const PickerControlEntry* getTexturePicker(ETextureIndex index) const { return mTextureCtrls.lookup(index); }
	const PickerControlEntry* getColorSwatch(ETextureIndex index) const { return mColorSwatchCtrls.lookup(index); }
};

LLEditWearableDictionary::LLEditWearableDictionary()
{

}

//virtual 
LLEditWearableDictionary::~LLEditWearableDictionary()
{
}

LLEditWearableDictionary::Wearables::Wearables()
{
	addEntry(WT_SHAPE, new WearableEntry(WT_SHAPE,"edit_shape_title","shape_desc_text",0,0,9,	SUBPART_SHAPE_HEAD,	SUBPART_SHAPE_EYES,	SUBPART_SHAPE_EARS,	SUBPART_SHAPE_NOSE,	SUBPART_SHAPE_MOUTH, SUBPART_SHAPE_CHIN, SUBPART_SHAPE_TORSO, SUBPART_SHAPE_LEGS, SUBPART_SHAPE_WHOLE));
	addEntry(WT_SKIN, new WearableEntry(WT_SKIN,"edit_skin_title","skin_desc_text",0,3,4, TEX_HEAD_BODYPAINT, TEX_UPPER_BODYPAINT, TEX_LOWER_BODYPAINT, SUBPART_SKIN_COLOR, SUBPART_SKIN_FACEDETAIL, SUBPART_SKIN_MAKEUP, SUBPART_SKIN_BODYDETAIL));
	addEntry(WT_HAIR, new WearableEntry(WT_HAIR,"edit_hair_title","hair_desc_text",0,1,4, TEX_HAIR, SUBPART_HAIR_COLOR,	SUBPART_HAIR_STYLE,	SUBPART_HAIR_EYEBROWS, SUBPART_HAIR_FACIAL));
	addEntry(WT_EYES, new WearableEntry(WT_EYES,"edit_eyes_title","eyes_desc_text",0,1,1, TEX_EYES_IRIS, SUBPART_EYES));
	addEntry(WT_SHIRT, new WearableEntry(WT_SHIRT,"edit_shirt_title","shirt_desc_text",1,1,1, TEX_UPPER_SHIRT, TEX_UPPER_SHIRT, SUBPART_SHIRT));
	addEntry(WT_PANTS, new WearableEntry(WT_PANTS,"edit_pants_title","pants_desc_text",1,1,1, TEX_LOWER_PANTS, TEX_LOWER_PANTS, SUBPART_PANTS));
	addEntry(WT_SHOES, new WearableEntry(WT_SHOES,"edit_shoes_title","shoes_desc_text",1,1,1, TEX_LOWER_SHOES, TEX_LOWER_SHOES, SUBPART_SHOES));
	addEntry(WT_SOCKS, new WearableEntry(WT_SOCKS,"edit_socks_title","socks_desc_text",1,1,1, TEX_LOWER_SOCKS, TEX_LOWER_SOCKS, SUBPART_SOCKS));
	addEntry(WT_JACKET, new WearableEntry(WT_JACKET,"edit_jacket_title","jacket_desc_text",1,2,1, TEX_UPPER_JACKET, TEX_UPPER_JACKET, TEX_LOWER_JACKET, SUBPART_JACKET));
	addEntry(WT_GLOVES, new WearableEntry(WT_GLOVES,"edit_gloves_title","gloves_desc_text",1,1,1, TEX_UPPER_GLOVES, TEX_UPPER_GLOVES, SUBPART_GLOVES));
	addEntry(WT_UNDERSHIRT, new WearableEntry(WT_UNDERSHIRT,"edit_undershirt_title","undershirt_desc_text",1,1,1, TEX_UPPER_UNDERSHIRT, TEX_UPPER_UNDERSHIRT, SUBPART_UNDERSHIRT));
	addEntry(WT_UNDERPANTS, new WearableEntry(WT_UNDERPANTS,"edit_underpants_title","underpants_desc_text",1,1,1, TEX_LOWER_UNDERPANTS, TEX_LOWER_UNDERPANTS, SUBPART_UNDERPANTS));
	addEntry(WT_SKIRT, new WearableEntry(WT_SKIRT,"edit_skirt_title","skirt_desc_text",1,1,1, TEX_SKIRT, TEX_SKIRT, SUBPART_SKIRT));
	addEntry(WT_ALPHA, new WearableEntry(WT_ALPHA,"edit_alpha_title","alpha_desc_text",0,5,1, TEX_LOWER_ALPHA, TEX_UPPER_ALPHA, TEX_HEAD_ALPHA, TEX_EYES_ALPHA, TEX_HAIR_ALPHA, SUBPART_ALPHA));
	addEntry(WT_TATTOO, new WearableEntry(WT_TATTOO,"edit_tattoo_title","tattoo_desc_text",0,3,1, TEX_LOWER_TATTOO, TEX_UPPER_TATTOO, TEX_HEAD_TATTOO, SUBPART_TATTOO));
}

LLEditWearableDictionary::WearableEntry::WearableEntry(EWearableType type,
					  const std::string &title,
					  const std::string &desc_title,
					  U8 num_color_swatches,
					  U8 num_texture_pickers,
					  U8 num_subparts, ... ) :
	LLDictionaryEntry(title),
	mWearableType(type),
	mTitle(title),
	mDescTitle(desc_title)
{
	va_list argp;
	va_start(argp, num_subparts);

	for (U8 i = 0; i < num_color_swatches; ++i)
	{
		ETextureIndex index = (ETextureIndex)va_arg(argp,int);
		mColorSwatchCtrls.push_back(index);
	}

	for (U8 i = 0; i < num_texture_pickers; ++i)
	{
		ETextureIndex index = (ETextureIndex)va_arg(argp,int);
		mTextureCtrls.push_back(index);
	}

	for (U8 i = 0; i < num_subparts; ++i)
	{
		ESubpart part = (ESubpart)va_arg(argp,int);
		mSubparts.push_back(part);
	}
}

LLEditWearableDictionary::Subparts::Subparts()
{
	addEntry(SUBPART_SHAPE_WHOLE, new SubpartEntry(SUBPART_SHAPE_WHOLE, "mPelvis", "shape_body","shape_body_param_list", "shape_body_tab", LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-2.5f, 0.5f, 0.8f),SEX_BOTH));
	addEntry(SUBPART_SHAPE_HEAD, new SubpartEntry(SUBPART_SHAPE_HEAD, "mHead", "shape_head", "shape_head_param_list", "shape_head_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SHAPE_EYES, new SubpartEntry(SUBPART_SHAPE_EYES, "mHead", "shape_eyes", "shape_eyes_param_list", "shape_eyes_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SHAPE_EARS, new SubpartEntry(SUBPART_SHAPE_EARS, "mHead", "shape_ears", "shape_ears_param_list", "shape_ears_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SHAPE_NOSE, new SubpartEntry(SUBPART_SHAPE_NOSE, "mHead", "shape_nose", "shape_nose_param_list", "shape_nose_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SHAPE_MOUTH, new SubpartEntry(SUBPART_SHAPE_MOUTH, "mHead", "shape_mouth", "shape_mouth_param_list", "shape_mouth_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SHAPE_CHIN, new SubpartEntry(SUBPART_SHAPE_CHIN, "mHead", "shape_chin", "shape_chin_param_list", "shape_chin_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SHAPE_TORSO, new SubpartEntry(SUBPART_SHAPE_TORSO, "mTorso", "shape_torso", "shape_torso_param_list", "shape_torso_tab", LLVector3d(0.f, 0.f, 0.3f), LLVector3d(-1.f, 0.15f, 0.3f),SEX_BOTH));
	addEntry(SUBPART_SHAPE_LEGS, new SubpartEntry(SUBPART_SHAPE_LEGS, "mPelvis", "shape_legs", "shape_legs_param_list", "shape_legs_tab", LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f),SEX_BOTH));

	addEntry(SUBPART_SKIN_COLOR, new SubpartEntry(SUBPART_SKIN_COLOR, "mHead", "skin_color", "skin_color_param_list", "skin_color_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SKIN_FACEDETAIL, new SubpartEntry(SUBPART_SKIN_FACEDETAIL, "mHead", "skin_facedetail", "skin_face_param_list", "skin_face_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SKIN_MAKEUP, new SubpartEntry(SUBPART_SKIN_MAKEUP, "mHead", "skin_makeup", "skin_makeup_param_list", "skin_makeup_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_SKIN_BODYDETAIL, new SubpartEntry(SUBPART_SKIN_BODYDETAIL, "mPelvis", "skin_bodydetail", "skin_body_param_list", "skin_body_tab", LLVector3d(0.f, 0.f, -0.2f), LLVector3d(-2.5f, 0.5f, 0.5f),SEX_BOTH));

	addEntry(SUBPART_HAIR_COLOR, new SubpartEntry(SUBPART_HAIR_COLOR, "mHead", "hair_color", "hair_color_param_list", "hair_color_tab", LLVector3d(0.f, 0.f, 0.10f), LLVector3d(-0.4f, 0.05f, 0.10f),SEX_BOTH));
	addEntry(SUBPART_HAIR_STYLE, new SubpartEntry(SUBPART_HAIR_STYLE, "mHead", "hair_style", "hair_style_param_list", "hair_style_tab", LLVector3d(0.f, 0.f, 0.10f), LLVector3d(-0.4f, 0.05f, 0.10f),SEX_BOTH));
	addEntry(SUBPART_HAIR_EYEBROWS, new SubpartEntry(SUBPART_HAIR_EYEBROWS, "mHead", "hair_eyebrows", "hair_eyebrows_param_list", "hair_eyebrows_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));
	addEntry(SUBPART_HAIR_FACIAL, new SubpartEntry(SUBPART_HAIR_FACIAL, "mHead", "hair_facial", "hair_facial_param_list", "hair_facial_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_MALE));

	addEntry(SUBPART_EYES, new SubpartEntry(SUBPART_EYES, "mHead", "eyes", "eyes_main_param_list", "eyes_main_tab", LLVector3d(0.f, 0.f, 0.05f), LLVector3d(-0.5f, 0.05f, 0.07f),SEX_BOTH));

	addEntry(SUBPART_SHIRT, new SubpartEntry(SUBPART_SHIRT, "mTorso", "shirt", "shirt_main_param_list", "shirt_main_tab", LLVector3d(0.f, 0.f, 0.3f), LLVector3d(-1.f, 0.15f, 0.3f),SEX_BOTH));
	addEntry(SUBPART_PANTS, new SubpartEntry(SUBPART_PANTS, "mPelvis", "pants", "pants_main_param_list", "pants_main_tab", LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f),SEX_BOTH));
	addEntry(SUBPART_SHOES, new SubpartEntry(SUBPART_SHOES, "mPelvis", "shoes", "shoes_main_param_list", "shoes_main_tab", LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f),SEX_BOTH));
	addEntry(SUBPART_SOCKS, new SubpartEntry(SUBPART_SOCKS, "mPelvis", "socks", "socks_main_param_list", "socks_main_tab", LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f),SEX_BOTH));
	addEntry(SUBPART_JACKET, new SubpartEntry(SUBPART_JACKET, "mTorso", "jacket", "jacket_main_param_list", "jacket_main_tab", LLVector3d(0.f, 0.f, 0.f), LLVector3d(-2.f, 0.1f, 0.3f),SEX_BOTH));
	addEntry(SUBPART_SKIRT, new SubpartEntry(SUBPART_SKIRT, "mPelvis", "skirt", "skirt_main_param_list", "skirt_main_tab", LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f),SEX_BOTH));
	addEntry(SUBPART_GLOVES, new SubpartEntry(SUBPART_GLOVES, "mTorso", "gloves", "gloves_main_param_list", "gloves_main_tab", LLVector3d(0.f, 0.f, 0.f), LLVector3d(-1.f, 0.15f, 0.f),SEX_BOTH));
	addEntry(SUBPART_UNDERSHIRT, new SubpartEntry(SUBPART_UNDERSHIRT, "mTorso", "undershirt", "undershirt_main_param_list", "undershirt_main_tab", LLVector3d(0.f, 0.f, 0.3f), LLVector3d(-1.f, 0.15f, 0.3f),SEX_BOTH));
	addEntry(SUBPART_UNDERPANTS, new SubpartEntry(SUBPART_UNDERPANTS, "mPelvis", "underpants", "underpants_main_param_list", "underpants_main_tab", LLVector3d(0.f, 0.f, -0.5f), LLVector3d(-1.6f, 0.15f, -0.5f),SEX_BOTH));
	addEntry(SUBPART_ALPHA, new SubpartEntry(SUBPART_ALPHA, "mPelvis", "alpha", "alpha_main_param_list", "alpha_main_tab", LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-2.5f, 0.5f, 0.8f),SEX_BOTH));
	addEntry(SUBPART_TATTOO, new SubpartEntry(SUBPART_TATTOO, "mPelvis", "tattoo", "tattoo_main_param_list", "tattoo_main_tab", LLVector3d(0.f, 0.f, 0.1f), LLVector3d(-2.5f, 0.5f, 0.8f),SEX_BOTH));
}

LLEditWearableDictionary::SubpartEntry::SubpartEntry(ESubpart part,
					 const std::string &joint,
					 const std::string &edit_group,
					 const std::string &param_list,
					 const std::string &accordion_tab,
					 const LLVector3d  &target_offset,
					 const LLVector3d  &camera_offset,
					 const ESex 	   &sex) :
	LLDictionaryEntry(edit_group),
	mSubpart(part),
	mTargetJoint(joint),
	mEditGroup(edit_group),
	mParamList(param_list),
	mAccordionTab(accordion_tab),
	mTargetOffset(target_offset),
	mCameraOffset(camera_offset),
	mSex(sex)
{
}

LLEditWearableDictionary::ColorSwatchCtrls::ColorSwatchCtrls()
{
	addEntry ( TEX_UPPER_SHIRT,  new PickerControlEntry (TEX_UPPER_SHIRT, "Color/Tint" ));
	addEntry ( TEX_LOWER_PANTS,  new PickerControlEntry (TEX_LOWER_PANTS, "Color/Tint" ));
	addEntry ( TEX_LOWER_SHOES,  new PickerControlEntry (TEX_LOWER_SHOES, "Color/Tint" ));
	addEntry ( TEX_LOWER_SOCKS,  new PickerControlEntry (TEX_LOWER_SOCKS, "Color/Tint" ));
	addEntry ( TEX_UPPER_JACKET, new PickerControlEntry (TEX_UPPER_JACKET, "Color/Tint" ));
	addEntry ( TEX_SKIRT,  new PickerControlEntry (TEX_SKIRT, "Color/Tint" ));
	addEntry ( TEX_UPPER_GLOVES, new PickerControlEntry (TEX_UPPER_GLOVES, "Color/Tint" ));
	addEntry ( TEX_UPPER_UNDERSHIRT, new PickerControlEntry (TEX_UPPER_UNDERSHIRT, "Color/Tint" ));
	addEntry ( TEX_LOWER_UNDERPANTS, new PickerControlEntry (TEX_LOWER_UNDERPANTS, "Color/Tint" ));
}

LLEditWearableDictionary::TextureCtrls::TextureCtrls()
{
	addEntry ( TEX_HEAD_BODYPAINT,  new PickerControlEntry (TEX_HEAD_BODYPAINT,  "Head Tattoos", LLUUID::null, TRUE ));
	addEntry ( TEX_UPPER_BODYPAINT, new PickerControlEntry (TEX_UPPER_BODYPAINT, "Upper Tattoos", LLUUID::null, TRUE ));
	addEntry ( TEX_LOWER_BODYPAINT, new PickerControlEntry (TEX_LOWER_BODYPAINT, "Lower Tattoos", LLUUID::null, TRUE ));
	addEntry ( TEX_HAIR, new PickerControlEntry (TEX_HAIR, "Texture", LLUUID( gSavedSettings.getString( "UIImgDefaultHairUUID" ) ), FALSE ));
	addEntry ( TEX_EYES_IRIS, new PickerControlEntry (TEX_EYES_IRIS, "Iris", LLUUID( gSavedSettings.getString( "UIImgDefaultEyesUUID" ) ), FALSE ));
	addEntry ( TEX_UPPER_SHIRT, new PickerControlEntry (TEX_UPPER_SHIRT, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultShirtUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_PANTS, new PickerControlEntry (TEX_LOWER_PANTS, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultPantsUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_SHOES, new PickerControlEntry (TEX_LOWER_SHOES, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultShoesUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_SOCKS, new PickerControlEntry (TEX_LOWER_SOCKS, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultSocksUUID" ) ), FALSE ));
	addEntry ( TEX_UPPER_JACKET, new PickerControlEntry (TEX_UPPER_JACKET, "Upper Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultJacketUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_JACKET, new PickerControlEntry (TEX_LOWER_JACKET, "Lower Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultJacketUUID" ) ), FALSE ));
	addEntry ( TEX_SKIRT, new PickerControlEntry (TEX_SKIRT, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultSkirtUUID" ) ), FALSE ));
	addEntry ( TEX_UPPER_GLOVES, new PickerControlEntry (TEX_UPPER_GLOVES, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultGlovesUUID" ) ), FALSE ));
	addEntry ( TEX_UPPER_UNDERSHIRT, new PickerControlEntry (TEX_UPPER_UNDERSHIRT, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultUnderwearUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_UNDERPANTS, new PickerControlEntry (TEX_LOWER_UNDERPANTS, "Fabric", LLUUID( gSavedSettings.getString( "UIImgDefaultUnderwearUUID" ) ), FALSE ));
	addEntry ( TEX_LOWER_ALPHA, new PickerControlEntry (TEX_LOWER_ALPHA, "Lower Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_UPPER_ALPHA, new PickerControlEntry (TEX_UPPER_ALPHA, "Upper Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_HEAD_ALPHA, new PickerControlEntry (TEX_HEAD_ALPHA, "Head Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_EYES_ALPHA, new PickerControlEntry (TEX_EYES_ALPHA, "Eye Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_HAIR_ALPHA, new PickerControlEntry (TEX_HAIR_ALPHA, "Hair Alpha", LLUUID( gSavedSettings.getString( "UIImgDefaultAlphaUUID" ) ), TRUE ));
	addEntry ( TEX_LOWER_TATTOO, new PickerControlEntry (TEX_LOWER_TATTOO, "Lower Tattoo", LLUUID::null, TRUE ));
	addEntry ( TEX_UPPER_TATTOO, new PickerControlEntry (TEX_UPPER_TATTOO, "Upper Tattoo", LLUUID::null, TRUE ));
	addEntry ( TEX_HEAD_TATTOO, new PickerControlEntry (TEX_HEAD_TATTOO, "Head Tattoo", LLUUID::null, TRUE ));
}

LLEditWearableDictionary::PickerControlEntry::PickerControlEntry(ETextureIndex tex_index,
					 const std::string name,
					 const LLUUID default_image_id,
					 const bool allow_no_texture) :
	LLDictionaryEntry(name),
	mTextureIndex(tex_index),
	mControlName(name),
	mDefaultImageId(default_image_id),
	mAllowNoTexture(allow_no_texture)
{
}

// Helper functions.
static const texture_vec_t null_texture_vec;

// Specializations of this template function return a vector of texture indexes of particular control type
// (i.e. LLColorSwatchCtrl or LLTextureCtrl) which are contained in given WearableEntry.
template <typename T>
const texture_vec_t&
get_pickers_indexes(const LLEditWearableDictionary::WearableEntry *wearable_entry) { return null_texture_vec; }

// Specializations of this template function return picker control entry for particular control type.
template <typename T>
const LLEditWearableDictionary::PickerControlEntry*
get_picker_entry (const ETextureIndex index) { return NULL; }

typedef boost::function<void(LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry*)> function_t;

typedef struct PickerControlEntryNamePredicate
{
	PickerControlEntryNamePredicate(const std::string name) : mName (name) {};
	bool operator()(const LLEditWearableDictionary::PickerControlEntry* entry) const
	{
		return (entry && entry->mName == mName);
	}
private:
	const std::string mName;
} PickerControlEntryNamePredicate;

// A full specialization of get_pickers_indexes for LLColorSwatchCtrl
template <>
const texture_vec_t&
get_pickers_indexes<LLColorSwatchCtrl> (const LLEditWearableDictionary::WearableEntry *wearable_entry)
{
	if (!wearable_entry)
	{
		llwarns << "could not get LLColorSwatchCtrl indexes for null wearable entry." << llendl;
		return null_texture_vec;
	}
	return wearable_entry->mColorSwatchCtrls;
}

// A full specialization of get_pickers_indexes for LLTextureCtrl
template <>
const texture_vec_t&
get_pickers_indexes<LLTextureCtrl> (const LLEditWearableDictionary::WearableEntry *wearable_entry)
{
	if (!wearable_entry)
	{
		llwarns << "could not get LLTextureCtrl indexes for null wearable entry." << llendl;
		return null_texture_vec;
	}
	return wearable_entry->mTextureCtrls;
}

// A full specialization of get_picker_entry for LLColorSwatchCtrl
template <>
const LLEditWearableDictionary::PickerControlEntry*
get_picker_entry<LLColorSwatchCtrl> (const ETextureIndex index)
{
	return LLEditWearableDictionary::getInstance()->getColorSwatch(index);
}

// A full specialization of get_picker_entry for LLTextureCtrl
template <>
const LLEditWearableDictionary::PickerControlEntry*
get_picker_entry<LLTextureCtrl> (const ETextureIndex index)
{
	return LLEditWearableDictionary::getInstance()->getTexturePicker(index);
}

template <typename CtrlType, class Predicate>
const LLEditWearableDictionary::PickerControlEntry*
find_picker_ctrl_entry_if(EWearableType type, const Predicate pred)
{
	const LLEditWearableDictionary::WearableEntry *wearable_entry
		= LLEditWearableDictionary::getInstance()->getWearable(type);
	if (!wearable_entry)
	{
		llwarns << "could not get wearable dictionary entry for wearable of type: " << type << llendl;
		return NULL;
	}
	const texture_vec_t& indexes = get_pickers_indexes<CtrlType>(wearable_entry);
	for (texture_vec_t::const_iterator
			 iter = indexes.begin(),
			 iter_end = indexes.end();
		 iter != iter_end; ++iter)
	{
		const ETextureIndex te = *iter;
		const LLEditWearableDictionary::PickerControlEntry*	entry
			= get_picker_entry<CtrlType>(te);
		if (!entry)
		{
			llwarns << "could not get picker dictionary entry (" << te << ") for wearable of type: " << type << llendl;
			continue;
		}
		if (pred(entry))
		{
			return entry;
		}
	}
	return NULL;
}

template <typename CtrlType>
void
for_each_picker_ctrl_entry(LLPanel* panel, EWearableType type, function_t fun)
{
	if (!panel)
	{
		llwarns << "the panel wasn't passed for wearable of type: " << type << llendl;
		return;
	}
	const LLEditWearableDictionary::WearableEntry *wearable_entry
		= LLEditWearableDictionary::getInstance()->getWearable(type);
	if (!wearable_entry)
	{
		llwarns << "could not get wearable dictionary entry for wearable of type: " << type << llendl;
		return;
	}
	const texture_vec_t& indexes = get_pickers_indexes<CtrlType>(wearable_entry);
	for (texture_vec_t::const_iterator
			 iter = indexes.begin(),
			 iter_end = indexes.end();
		 iter != iter_end; ++iter)
	{
		const ETextureIndex te = *iter;
		const LLEditWearableDictionary::PickerControlEntry*	entry
			= get_picker_entry<CtrlType>(te);
		if (!entry)
		{
			llwarns << "could not get picker dictionary entry (" << te << ") for wearable of type: " << type << llendl;
			continue;
		}
		fun (panel, entry);
	}
}

// The helper functions for pickers management
static void init_color_swatch_ctrl(LLPanelEditWearable* self, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLColorSwatchCtrl* color_swatch_ctrl = panel->getChild<LLColorSwatchCtrl>(entry->mControlName);
	if (color_swatch_ctrl)
	{
		color_swatch_ctrl->setOriginal(self->getWearable()->getClothesColor(entry->mTextureIndex));
	}
}

static void init_texture_ctrl(LLPanelEditWearable* self, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLTextureCtrl* texture_ctrl = panel->getChild<LLTextureCtrl>(entry->mControlName);
	if (texture_ctrl)
	{
		texture_ctrl->setDefaultImageAssetID(entry->mDefaultImageId);
		texture_ctrl->setAllowNoTexture(entry->mAllowNoTexture);
		// Don't allow (no copy) or (notransfer) textures to be selected.
		texture_ctrl->setImmediateFilterPermMask(PERM_NONE);
		texture_ctrl->setNonImmediateFilterPermMask(PERM_NONE);
	}
}

static void update_color_swatch_ctrl(LLPanelEditWearable* self, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLColorSwatchCtrl* color_swatch_ctrl = panel->getChild<LLColorSwatchCtrl>(entry->mControlName);
	if (color_swatch_ctrl)
	{
		color_swatch_ctrl->set(self->getWearable()->getClothesColor(entry->mTextureIndex));
	}
}

static void update_texture_ctrl(LLPanelEditWearable* self, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLTextureCtrl* texture_ctrl = panel->getChild<LLTextureCtrl>(entry->mControlName);
	if (texture_ctrl)
	{
		LLUUID new_id;
		LLLocalTextureObject *lto = self->getWearable()->getLocalTextureObject(entry->mTextureIndex);
		if( lto && (lto->getID() != IMG_DEFAULT_AVATAR) )
		{
			new_id = lto->getID();
		}
		else
		{
			new_id = LLUUID::null;
		}
		LLUUID old_id = texture_ctrl->getImageAssetID();
		if (old_id != new_id)
		{
			// texture has changed, close the floater to avoid DEV-22461
			texture_ctrl->closeDependentFloater();
		}
		texture_ctrl->setImageAssetID(new_id);
	}
}

static void set_enabled_color_swatch_ctrl(bool enabled, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLColorSwatchCtrl* color_swatch_ctrl = panel->getChild<LLColorSwatchCtrl>(entry->mControlName);
	if (color_swatch_ctrl)
	{
		color_swatch_ctrl->setEnabled(enabled);
	}
}

static void set_enabled_texture_ctrl(bool enabled, LLPanel* panel, const LLEditWearableDictionary::PickerControlEntry* entry)
{
	LLTextureCtrl* texture_ctrl = panel->getChild<LLTextureCtrl>(entry->mControlName);
	if (texture_ctrl)
	{
		texture_ctrl->setEnabled(enabled);
	}
}

// LLPanelEditWearable

LLPanelEditWearable::LLPanelEditWearable()
	: LLPanel()
	, mWearablePtr(NULL)
	, mWearableItem(NULL)
{
	mCommitCallbackRegistrar.add("ColorSwatch.Commit", boost::bind(&LLPanelEditWearable::onColorSwatchCommit, this, _1));
	mCommitCallbackRegistrar.add("TexturePicker.Commit", boost::bind(&LLPanelEditWearable::onTexturePickerCommit, this, _1));
}

//virtual
LLPanelEditWearable::~LLPanelEditWearable()
{

}

// virtual 
BOOL LLPanelEditWearable::postBuild()
{
	// buttons
	mBtnRevert = getChild<LLButton>("revert_button");
	mBtnRevert->setClickedCallback(boost::bind(&LLPanelEditWearable::onRevertButtonClicked, this));

	mBtnBack = getChild<LLButton>("back_btn");
	// handled at appearance panel level?
	//mBtnBack->setClickedCallback(boost::bind(&LLPanelEditWearable::onBackButtonClicked, this));

	mTextEditor = getChild<LLTextEditor>("description");

	mPanelTitle = getChild<LLTextBox>("edit_wearable_title");
	mDescTitle = getChild<LLTextBox>("description_text");

	// The following panels will be shown/hidden based on what wearable we're editing
	// body parts
	mPanelShape = getChild<LLPanel>("edit_shape_panel");
	mPanelSkin = getChild<LLPanel>("edit_skin_panel");
	mPanelEyes = getChild<LLPanel>("edit_eyes_panel");
	mPanelHair = getChild<LLPanel>("edit_hair_panel");

	//clothes
	mPanelShirt = getChild<LLPanel>("edit_shirt_panel");
	mPanelPants = getChild<LLPanel>("edit_pants_panel");
	mPanelShoes = getChild<LLPanel>("edit_shoes_panel");
	mPanelSocks = getChild<LLPanel>("edit_socks_panel");
	mPanelJacket = getChild<LLPanel>("edit_jacket_panel");
	mPanelGloves = getChild<LLPanel>("edit_gloves_panel");
	mPanelUndershirt = getChild<LLPanel>("edit_undershirt_panel");
	mPanelUnderpants = getChild<LLPanel>("edit_underpants_panel");
	mPanelSkirt = getChild<LLPanel>("edit_skirt_panel");
	mPanelAlpha = getChild<LLPanel>("edit_alpha_panel");
	mPanelTattoo = getChild<LLPanel>("edit_tattoo_panel");

	mWearablePtr = NULL;

	return TRUE;
}

// virtual 
// LLUICtrl
BOOL LLPanelEditWearable::isDirty() const
{
	BOOL isDirty = FALSE;
	if (mWearablePtr)
	{
		if (mWearablePtr->isDirty() ||
			mWearablePtr->getName().compare(mTextEditor->getText()) != 0)
		{
			isDirty = TRUE;
		}
	}
	return isDirty;
}
//virtual
void LLPanelEditWearable::draw()
{
	updateVerbs();
	if (getWearable())
	{
		updatePanelPickerControls(getWearable()->getType());
	}

	LLPanel::draw();
}

void LLPanelEditWearable::setWearable(LLWearable *wearable)
{
	showWearable(mWearablePtr, FALSE);
	mWearablePtr = wearable;
	showWearable(mWearablePtr, TRUE);

	initializePanel();
}

//static 
void LLPanelEditWearable::onRevertButtonClicked(void* userdata)
{
	LLPanelEditWearable *panel = (LLPanelEditWearable*) userdata;
	panel->revertChanges();
}

void LLPanelEditWearable::onTexturePickerCommit(const LLUICtrl* ctrl)
{
	const LLTextureCtrl* texture_ctrl = dynamic_cast<const LLTextureCtrl*>(ctrl);
	if (!texture_ctrl)
	{
		llwarns << "got commit signal from not LLTextureCtrl." << llendl;
		return;
	}

	if (getWearable())
	{
		EWearableType type = getWearable()->getType();
		const PickerControlEntryNamePredicate name_pred(texture_ctrl->getName());
		const LLEditWearableDictionary::PickerControlEntry* entry
			= find_picker_ctrl_entry_if<LLTextureCtrl, PickerControlEntryNamePredicate>(type, name_pred);
		if (entry)
		{
			// Set the new version
			LLViewerFetchedTexture* image = LLViewerTextureManager::getFetchedTexture(texture_ctrl->getImageAssetID());
			if( image->getID().isNull() )
			{
				image = LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
			}
			if (getWearable())
			{
				U32 index = gAgentWearables.getWearableIndex(getWearable());
				gAgentAvatarp->setLocalTexture(entry->mTextureIndex, image, FALSE, index);
				LLVisualParamHint::requestHintUpdates();
				gAgentAvatarp->wearableUpdated(type, FALSE);
			}
		}
		else
		{
			llwarns << "could not get texture picker dictionary entry for wearable of type: " << type << llendl;
		}
	}
}

void LLPanelEditWearable::onColorSwatchCommit(const LLUICtrl* ctrl)
{
	if (getWearable())
	{
		EWearableType type = getWearable()->getType();
		const PickerControlEntryNamePredicate name_pred(ctrl->getName());
		const LLEditWearableDictionary::PickerControlEntry* entry
			= find_picker_ctrl_entry_if<LLColorSwatchCtrl, PickerControlEntryNamePredicate>(type, name_pred);
		if (entry)
		{
			const LLColor4& old_color = getWearable()->getClothesColor(entry->mTextureIndex);
			const LLColor4& new_color = LLColor4(ctrl->getValue());
			if( old_color != new_color )
			{
				getWearable()->setClothesColor(entry->mTextureIndex, new_color, TRUE);
				LLVisualParamHint::requestHintUpdates();
				gAgentAvatarp->wearableUpdated(getWearable()->getType(), FALSE);
			}
		}
		else
		{
			llwarns << "could not get color swatch dictionary entry for wearable of type: " << type << llendl;
		}
	}
}

void LLPanelEditWearable::updatePanelPickerControls(EWearableType type)
{
	LLPanel* panel = getPanel(type);
	if (!panel)
		return;

	bool is_modifiable = false;
	bool is_complete   = false;
	bool is_copyable   = false;

	if(mWearableItem)
	{
		const LLPermissions& perm = mWearableItem->getPermissions();
		is_modifiable = perm.allowModifyBy(gAgent.getID(), gAgent.getGroupID());
		is_copyable = perm.allowCopyBy(gAgent.getID(), gAgent.getGroupID());
		is_complete = mWearableItem->isFinished();
	}

	if (is_modifiable && is_complete)
	{
		// Update picker controls
		for_each_picker_ctrl_entry <LLColorSwatchCtrl> (panel, type, boost::bind(update_color_swatch_ctrl, this, _1, _2));
		for_each_picker_ctrl_entry <LLTextureCtrl>     (panel, type, boost::bind(update_texture_ctrl, this, _1, _2));
	}

	if (!is_modifiable || !is_complete || !is_copyable)
	{
		// Disable controls
		for_each_picker_ctrl_entry <LLColorSwatchCtrl> (panel, type, boost::bind(set_enabled_color_swatch_ctrl, false, _1, _2));
		for_each_picker_ctrl_entry <LLTextureCtrl>     (panel, type, boost::bind(set_enabled_texture_ctrl, false, _1, _2));
	}
}

void LLPanelEditWearable::saveChanges()
{
	if (!mWearablePtr || !isDirty())
	{
		// do nothing if no unsaved changes
		return;
	}

	U32 index = gAgentWearables.getWearableIndex(mWearablePtr);
	
	if (mWearablePtr->getName().compare(mTextEditor->getText()) != 0)
	{
		// the name of the wearable has changed, re-save wearable with new name
		gAgentWearables.saveWearableAs(mWearablePtr->getType(), index, mTextEditor->getText(), FALSE);
	}
	else
	{
		gAgentWearables.saveWearable(mWearablePtr->getType(), index);
	}
}

void LLPanelEditWearable::revertChanges()
{
	if (!mWearablePtr || !isDirty())
	{
		// no unsaved changes to revert
		return;
	}

	mWearablePtr->revertValues();
	mTextEditor->setText(mWearablePtr->getName());
}

void LLPanelEditWearable::showWearable(LLWearable* wearable, BOOL show)
{
	if (!wearable)
	{
		return;
	}

	mWearableItem = gInventory.getItem(mWearablePtr->getItemID());
	llassert(mWearableItem);

	EWearableType type = wearable->getType();
	LLPanel *targetPanel = NULL;
	std::string title;
	std::string description_title;

	const LLEditWearableDictionary::WearableEntry *entry = LLEditWearableDictionary::getInstance()->getWearable(type);
	if (!entry)
	{
		llwarns << "called LLPanelEditWearable::showWearable with an invalid wearable type! (" << type << ")" << llendl;
		return;
	}

	targetPanel = getPanel(type);
	title = getString(entry->mTitle);
	description_title = getString(entry->mDescTitle);

	targetPanel->setVisible(show);
	if (show)
	{
		mPanelTitle->setText(title);
		mDescTitle->setText(description_title);
	}

	// Update picker controls state
	for_each_picker_ctrl_entry <LLColorSwatchCtrl> (targetPanel, type, boost::bind(set_enabled_color_swatch_ctrl, show, _1, _2));
	for_each_picker_ctrl_entry <LLTextureCtrl>     (targetPanel, type, boost::bind(set_enabled_texture_ctrl, show, _1, _2));
}

void LLPanelEditWearable::initializePanel()
{
	if (!mWearablePtr)
	{
		// cannot initialize with a null reference.
		return;
	}

	EWearableType type = mWearablePtr->getType();

	// set name
	mTextEditor->setText(mWearablePtr->getName());

	// clear and rebuild visual param list
	const LLEditWearableDictionary::WearableEntry *wearable_entry = LLEditWearableDictionary::getInstance()->getWearable(type);
	if (!wearable_entry)
	{
		llwarns << "could not get wearable dictionary entry for wearable of type: " << type << llendl;
		return;
	}
	U8 num_subparts = wearable_entry->mSubparts.size();

	for (U8 index = 0; index < num_subparts; ++index)
	{
		// dive into data structures to get the panel we need
		ESubpart subpart_e = wearable_entry->mSubparts[index];
		const LLEditWearableDictionary::SubpartEntry *subpart_entry = LLEditWearableDictionary::getInstance()->getSubpart(subpart_e);

		if (!subpart_entry)
		{
			llwarns << "could not get wearable subpart dictionary entry for subpart: " << subpart_e << llendl;
			continue;
		}

		const std::string scrolling_panel = subpart_entry->mParamList;
		const std::string accordion_tab = subpart_entry->mAccordionTab;

		LLScrollingPanelList *panel_list = getChild<LLScrollingPanelList>(scrolling_panel);
		LLAccordionCtrlTab *tab = getChild<LLAccordionCtrlTab>(accordion_tab);

		if (!panel_list)
		{
			llwarns << "could not get scrolling panel list: " << scrolling_panel << llendl;
			continue;
		}

		if (!tab)
		{
			llwarns << "could not get llaccordionctrltab from UI with name: " << accordion_tab << llendl;
			continue;
		}

		// what edit group do we want to extract params for?
		const std::string edit_group = subpart_entry->mEditGroup;

		// storage for ordered list of visual params
		value_map_t sorted_params;
		getSortedParams(sorted_params, edit_group);

		buildParamList(panel_list, sorted_params, tab);

		updateScrollingPanelUI();
	}

	// initialize texture and color picker controls
	for_each_picker_ctrl_entry <LLColorSwatchCtrl> (getPanel(type), type, boost::bind(init_color_swatch_ctrl, this, _1, _2));
	for_each_picker_ctrl_entry <LLTextureCtrl>     (getPanel(type), type, boost::bind(init_texture_ctrl, this, _1, _2));

	updateVerbs();
}

void LLPanelEditWearable::updateScrollingPanelUI()
{
	// do nothing if we don't have a valid wearable we're editing
	if (mWearablePtr == NULL)
	{
		return;
	}

	EWearableType type = mWearablePtr->getType();
	LLPanel *panel = getPanel(type);

	if(panel && (mWearablePtr->getItemID().notNull()))
	{
		const LLEditWearableDictionary::WearableEntry *wearable_entry = LLEditWearableDictionary::getInstance()->getWearable(type);
		U8 num_subparts = wearable_entry->mSubparts.size();

		LLScrollingPanelParam::sUpdateDelayFrames = 0;
		for (U8 index = 0; index < num_subparts; ++index)
		{
			// dive into data structures to get the panel we need
			ESubpart subpart_e = wearable_entry->mSubparts[index];
			const LLEditWearableDictionary::SubpartEntry *subpart_entry = LLEditWearableDictionary::getInstance()->getSubpart(subpart_e);

			const std::string scrolling_panel = subpart_entry->mParamList;

			LLScrollingPanelList *panel_list = getChild<LLScrollingPanelList>(scrolling_panel);
	
			if (!panel_list)
			{
				llwarns << "could not get scrolling panel list: " << scrolling_panel << llendl;
				continue;
			}
			
			panel_list->updatePanels(TRUE);
		}
	}
}

LLPanel* LLPanelEditWearable::getPanel(EWearableType type)
{
	switch (type)
	{
		case WT_SHAPE:
			return mPanelShape;
			break;

		case WT_SKIN:
			return mPanelSkin;
			break;

		case WT_HAIR:
			return mPanelHair;
			break;

		case WT_EYES:
			return mPanelEyes;
			break;

		case WT_SHIRT:
			return mPanelShirt;
			break;

		case WT_PANTS:
			return mPanelPants;
			break;

		case WT_SHOES:
			return mPanelShoes;
			break;

		case WT_SOCKS:
			return mPanelSocks;
			break;

		case WT_JACKET:
			return mPanelJacket;
			break;

		case WT_GLOVES:
			return mPanelGloves;
			break;

		case WT_UNDERSHIRT:
			return mPanelUndershirt;
			break;

		case WT_UNDERPANTS:
			return mPanelUnderpants;
			break;

		case WT_SKIRT:
			return mPanelSkirt;
			break;

		case WT_ALPHA:
			return mPanelAlpha;
			break;

		case WT_TATTOO:
			return mPanelTattoo;
			break;
		default:
			break;
	}
	return NULL;
}

void LLPanelEditWearable::getSortedParams(value_map_t &sorted_params, const std::string &edit_group)
{
	LLWearable::visual_param_vec_t param_list;
	ESex avatar_sex = gAgentAvatarp->getSex();

	mWearablePtr->getVisualParams(param_list);

	for (LLWearable::visual_param_vec_t::iterator iter = param_list.begin();
		iter != param_list.end();
		++iter)
	{
		LLViewerVisualParam *param = (LLViewerVisualParam*) *iter;

		if (param->getID() == -1
			|| param->getGroup() != VISUAL_PARAM_GROUP_TWEAKABLE 
			|| param->getEditGroup() != edit_group 
			|| !(param->getSex() & avatar_sex))
		{
			continue;
		}

		value_map_t::value_type vt(-param->getDisplayOrder(), param);
		llassert( sorted_params.find(-param->getDisplayOrder()) == sorted_params.end() ); //check for duplicates
		sorted_params.insert(vt);
	}
}

void LLPanelEditWearable::buildParamList(LLScrollingPanelList *panel_list, value_map_t &sorted_params, LLAccordionCtrlTab *tab)
{
	// sorted_params is sorted according to magnitude of effect from
	// least to greatest.  Adding to the front of the child list
	// reverses that order.
	if( panel_list )
	{
		panel_list->clearPanels();
		value_map_t::iterator end = sorted_params.end();
		S32 height = 0;
		for(value_map_t::iterator it = sorted_params.begin(); it != end; ++it)
		{
			LLPanel::Params p;
			p.name("LLScrollingPanelParam");
			LLScrollingPanelParam* panel_param = new LLScrollingPanelParam( p, NULL, (*it).second, TRUE, this->getWearable());
			height = panel_list->addPanel( panel_param );
		}
	}
}

void LLPanelEditWearable::updateVerbs()
{
	bool can_copy = false;

	if(mWearableItem)
	{
		can_copy = mWearableItem->getPermissions().allowCopyBy(gAgentID);
	}

	BOOL is_dirty = isDirty();

	mBtnRevert->setEnabled(is_dirty);
	childSetEnabled("save_as_button", is_dirty && can_copy);
}

// EOF
