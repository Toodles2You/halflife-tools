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

#ifndef _SPRITE_H
#define _SPRITE_H

#define SPR_MAX_FRAMES 1000

typedef enum {
	ST_SYNC = 0,
	ST_RAND
} synctype_t;

typedef struct {
	int32_t ident;
	int32_t version;
	int32_t type;
	int32_t texFormat;
	float boundingradius;
	int32_t width;
	int32_t height;
	int32_t numframes;
	float beamlength;
	synctype_t synctype;
} dsprite_t;

#define SPR_VP_PARALLEL_UPRIGHT 0
#define SPR_FACING_UPRIGHT 1
#define SPR_VP_PARALLEL 2
#define SPR_ORIENTED 3
#define SPR_VP_PARALLEL_ORIENTED 4

#define SPR_NORMAL 0
#define SPR_ADDITIVE 1
#define SPR_INDEXALPHA 2
#define SPR_ALPHTEST 3

typedef struct {
	int32_t origin[2];
	int32_t width;
	int32_t height;
} dspriteframe_t;

typedef struct {
	int32_t numframes;
} dspritegroup_t;

typedef struct {
	float interval;
} dspriteinterval_t;

typedef enum {
	SPR_SINGLE = 0,
	SPR_GROUP
} spriteframetype_t;

typedef struct {
	spriteframetype_t type;
} dspriteframetype_t;

#endif /* _SPRITE_H */
