/*
===========================================================================
Copyright (C) 1996-2002, Valve LLC. All rights reserved.
Copyright (C) 2023 Toodles

This product contains software technology licensed from Id 
Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
All Rights Reserved.

Use, distribution, and modification of this source code and/or resulting
object code is restricted to non-commercial enhancements to products from
Valve LLC.  All other use, distribution, or modification is prohibited
without written permission from Valve LLC.
===========================================================================
*/

#ifdef __GNUC__
#define _GNU_SOURCE
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <memory.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include "decompile.h"
#include "studio.h"

static void decomp_calcbonevalue (
    FILE *mdl,
    int frame,
    float* out,
    float scale,
    int animindex)
{
    mstudioanimvalue_t value;
    mdl_seek (mdl, animindex, SEEK_SET);
    mdl_read (mdl, &value, sizeof (value));

    while (value.num.total <= frame)
    {
        frame -= value.num.total;
        animindex += sizeof (value) * (value.num.valid + 1);
        mdl_seek (mdl, animindex, SEEK_SET);
        mdl_read (mdl, &value, sizeof (value));
    }
    
    if (value.num.valid > frame)
    {
        mdl_seek (mdl, animindex + sizeof (value) * (frame + 1), SEEK_SET);
    }
    else
    {
        mdl_seek (mdl, animindex + sizeof (value) * value.num.valid, SEEK_SET);
    }

    mdl_read (mdl, &value, sizeof (value));
    *out += value.value * scale;
}

static void decomp_calcbone (
    FILE *mdl,
    int frame,
    mstudiobone_t *bone,
    mstudioanim_t *anim,
    vec3_t bone_pos,
    vec3_t bone_rot,
    int animindex)
{
    int i;
    
    for (i = 0; i < 3; ++i)
    {
        bone_pos[i] = bone->value[i];
        bone_rot[i] = bone->value[3 + i];
        
        if (anim->offset[i] != 0)
        {
            decomp_calcbonevalue (
                mdl,
                frame,
                &bone_pos[i],
                bone->scale[i],
                animindex + anim->offset[i]);
        }
        
        if (anim->offset[3 + i] != 0)
        {
            decomp_calcbonevalue (
                mdl,
                frame,
                &bone_rot[i],
                bone->scale[3 + i],
                animindex + anim->offset[3 + i]);
        }
    }
}

void decomp_studioanim (
    FILE *mdl,
    FILE *smd,
    studiohdr_t *header,
    mstudiobone_t *bones,
    int numframes,
    int animindex,
    const char *nodes)
{
    vec3_t *bone_pos = (vec3_t *)calloc (header->numbones, sizeof (*bone_pos));
    vec3_t *bone_rot = (vec3_t *)calloc (header->numbones, sizeof (*bone_rot));

    qc_write (smd, "version 1");
    qc_write (smd, nodes);

    int i, j;
    mstudioanim_t anim;
    mstudioanimvalue_t value;
    
    qc_write (smd, "skeleton");

    for (i = 0; i < numframes; ++i)
    {
        qc_writef (smd, "  time %i", i);

        for (j = 0; j < header->numbones; ++j)
        {
            mdl_seek (mdl, animindex + sizeof (anim) * j, SEEK_SET);
            mdl_read (mdl, &anim, sizeof (anim));

            decomp_calcbone (
                mdl,
                i,
                bones + j,
                &anim,
                bone_pos[j],
                bone_rot[j],
                animindex + sizeof (anim) * j);

            qc_writef (
                smd,
                "    %i %.06f %.06f %.06f %.06f %.06f %.06f",
                j,
                vec3_print (bone_pos[j]),
                vec3_print (bone_rot[j]));
        }
    }
    
    qc_write (smd, "end");
    
    free (bone_rot);
    free (bone_pos);
}
