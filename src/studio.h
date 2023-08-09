/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#ifndef _STUDIO_H
#define _STUDIO_H

#define MAXSTUDIOTRIANGLES 20000 // TODO: tune this
#define MAXSTUDIOVERTS 2048		 // TODO: tune this
#define MAXSTUDIOSEQUENCES 2048	 // total animation sequences -- KSH incremented
#define MAXSTUDIOSKINS 100		 // total textures
#define MAXSTUDIOSRCBONES 512	 // bones allowed at source movement
#define MAXSTUDIOBONES 128		 // total bones actually used
#define MAXSTUDIOMODELS 32		 // sub-models per model
#define MAXSTUDIOBODYPARTS 32
#define MAXSTUDIOGROUPS 16
#define MAXSTUDIOANIMATIONS 2048
#define MAXSTUDIOMESHES 256
#define MAXSTUDIOEVENTS 1024
#define MAXSTUDIOPIVOTS 256
#define MAXSTUDIOCONTROLLERS 8

typedef struct
{
	int32_t id;
	int32_t version;

	char name[64];
	int32_t length;

	vec3_t eyeposition; // ideal eye position
	vec3_t min;			// ideal movement hull size
	vec3_t max;

	vec3_t bbmin; // clipping bounding box
	vec3_t bbmax;

	int32_t flags;

	int32_t numbones; // bones
	int32_t boneindex;

	int32_t numbonecontrollers; // bone controllers
	int32_t bonecontrollerindex;

	int32_t numhitboxes; // complex bounding boxes
	int32_t hitboxindex;

	int32_t numseq; // animation sequences
	int32_t seqindex;

	int32_t numseqgroups; // demand loaded sequences
	int32_t seqgroupindex;

	int32_t numtextures; // raw textures
	int32_t textureindex;
	int32_t texturedataindex;

	int32_t numskinref; // replaceable textures
	int32_t numskinfamilies;
	int32_t skinindex;

	int32_t numbodyparts;
	int32_t bodypartindex;

	int32_t numattachments; // queryable attachable points
	int32_t attachmentindex;

	int32_t soundtable;
	int32_t soundindex;
	int32_t soundgroups;
	int32_t soundgroupindex;

	int32_t numtransitions; // animation node to animation node transition graph
	int32_t transitionindex;
} studiohdr_t;

// header for demand loaded sequence group data
typedef struct
{
	int32_t id;
	int32_t version;

	char name[64];
	int32_t length;
} studioseqhdr_t;

// bones
typedef struct
{
	char name[32];		   // bone name for symbolic links
	int32_t parent;			   // parent bone
	int32_t flags;			   // ??
	int32_t bonecontroller[6]; // bone controller index, -1 == none
	float value[6];		   // default DoF values
	float scale[6];		   // scale for delta DoF values
} mstudiobone_t;


// bone controllers
typedef struct
{
	int32_t bone; // -1 == 0
	int32_t type; // X, Y, Z, XR, YR, ZR, M
	float start;
	float end;
	int32_t rest;  // byte index value at rest
	int32_t index; // 0-3 user set controller, 4 mouth
} mstudiobonecontroller_t;

// intersection boxes
typedef struct
{
	int32_t bone;
	int32_t group;	  // intersection group
	vec3_t bbmin; // bounding box
	vec3_t bbmax;
} mstudiobbox_t;

#if !defined(CACHE_USER) && !defined(QUAKEDEF_H)
#define CACHE_USER
typedef struct cache_user_s
{
	void* data;
} cache_user_t;
#endif

//
// demand loaded sequence groups
//
typedef struct
{
	char label[32]; // textual name
	char name[64];	// file name
	int32_t unused1;	// was "cache"  - index pointer
	int32_t unused2;	// was "data" -  hack for group 0
} mstudioseqgroup_t;

// sequence descriptions
typedef struct
{
	char label[32]; // sequence label

	float fps; // frames per second
	int32_t flags; // looping/non-looping flags

	int32_t activity;
	int32_t actweight;

	int32_t numevents;
	int32_t eventindex;

	int32_t numframes; // number of frames per sequence

	int32_t numpivots; // number of foot pivots
	int32_t pivotindex;

	int32_t motiontype;
	int32_t motionbone;
	vec3_t linearmovement;
	int32_t automoveposindex;
	int32_t automoveangleindex;

	vec3_t bbmin; // per sequence bounding box
	vec3_t bbmax;

	int32_t numblends;
	int32_t animindex; // mstudioanim_t pointer relative to start of sequence group data
				   // [blend][bone][X, Y, Z, XR, YR, ZR]

	int32_t blendtype[2];	 // X, Y, Z, XR, YR, ZR
	float blendstart[2]; // starting value
	float blendend[2];	 // ending value
	int32_t blendparent;

	int32_t seqgroup; // sequence group for demand loading

	int32_t entrynode; // transition node at entry
	int32_t exitnode;  // transition node at exit
	int32_t nodeflags; // transition rules

	int32_t nextseq; // auto advancing sequences
} mstudioseqdesc_t;

// events
typedef struct 
{
	int32_t 				frame;
	int32_t					event;
	int32_t					type;
	char				options[64];
} mstudioevent_t;

// pivots
typedef struct
{
	vec3_t org; // pivot point
	int32_t start;
	int32_t end;
} mstudiopivot_t;

// attachment
typedef struct
{
	char name[32];
	int32_t type;
	int32_t bone;
	vec3_t org; // attachment point
	vec3_t vectors[3];
} mstudioattachment_t;

typedef struct
{
	unsigned short offset[6];
} mstudioanim_t;

// animation frames
typedef union
{
	struct
	{
		byte valid;
		byte total;
	} num;
	short value;
} mstudioanimvalue_t;



// body part index
typedef struct
{
	char name[64];
	int32_t nummodels;
	int32_t base;
	int32_t modelindex; // index into models array
} mstudiobodyparts_t;



// skin info
typedef struct
{
	char name[64];
	int32_t flags;
	int32_t width;
	int32_t height;
	int32_t index;
} mstudiotexture_t;


// skin families
// short	index[skinfamilies][skinref]

// studio models
typedef struct
{
	char name[64];

	int32_t type;

	float boundingradius;

	int32_t nummesh;
	int32_t meshindex;

	int32_t numverts;	   // number of unique vertices
	int32_t vertinfoindex; // vertex bone info
	int32_t vertindex;	   // vertex Vector
	int32_t numnorms;	   // number of unique surface normals
	int32_t norminfoindex; // normal bone info
	int32_t normindex;	   // normal Vector

	int32_t numgroups; // deformation groups
	int32_t groupindex;
} mstudiomodel_t;


// Vector	boundingbox[model][bone][2];	// complex intersection info


// meshes
typedef struct
{
	int32_t numtris;
	int32_t triindex;
	int32_t skinref;
	int32_t numnorms;  // per mesh normals
	int32_t normindex; // normal Vector
} mstudiomesh_t;

// triangles
#if 0
typedef struct 
{
	short				vertindex;		// index into vertex array
	short				normindex;		// index into normal array
	short				s,t;			// s,t position on skin
} mstudiotrivert_t;
#endif

// lighting options
#define STUDIO_NF_FLATSHADE 0x0001
#define STUDIO_NF_CHROME 0x0002
#define STUDIO_NF_FULLBRIGHT 0x0004
#define STUDIO_NF_NOMIPS 0x0008
#define STUDIO_NF_ALPHA 0x0010
#define STUDIO_NF_ADDITIVE 0x0020
#define STUDIO_NF_MASKED 0x0040

// motion flags
#define STUDIO_X 0x0001
#define STUDIO_Y 0x0002
#define STUDIO_Z 0x0004
#define STUDIO_XR 0x0008
#define STUDIO_YR 0x0010
#define STUDIO_ZR 0x0020
#define STUDIO_LX 0x0040
#define STUDIO_LY 0x0080
#define STUDIO_LZ 0x0100
#define STUDIO_AX 0x0200
#define STUDIO_AY 0x0400
#define STUDIO_AZ 0x0800
#define STUDIO_AXR 0x1000
#define STUDIO_AYR 0x2000
#define STUDIO_AZR 0x4000
#define STUDIO_TYPES 0x7FFF
#define STUDIO_RLOOP 0x8000 // controller that wraps shortest distance

// sequence flags
#define STUDIO_LOOPING 0x0001

// bone flags
#define STUDIO_HAS_NORMALS 0x0001
#define STUDIO_HAS_VERTICES 0x0002
#define STUDIO_HAS_BBOX 0x0004
#define STUDIO_HAS_CHROME 0x0008 // if any of the textures have chrome on them

#define RAD_TO_STUDIO (32768.0 / M_PI)
#define STUDIO_TO_RAD (M_PI / 32768.0)

#endif /* _STUDIOMDL_H */
