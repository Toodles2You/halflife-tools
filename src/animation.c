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

#include "studio.h"

static void decomp_calcbonevalue (
    FILE *seqgroup,
    int frame,
    float* out,
    float scale,
    int animindex)
{
    mstudioanimvalue_t value;
    mdl_seek (seqgroup, animindex, SEEK_SET);
    mdl_read (seqgroup, &value, sizeof (value));

    while (value.num.total <= frame)
    {
        frame -= value.num.total;
        animindex += sizeof (value) * (value.num.valid + 1);
        mdl_seek (seqgroup, animindex, SEEK_SET);
        mdl_read (seqgroup, &value, sizeof (value));
    }
    
    if (value.num.valid > frame)
    {
        mdl_seek (seqgroup, animindex + sizeof (value) * (frame + 1), SEEK_SET);
    }
    else
    {
        mdl_seek (seqgroup, animindex + sizeof (value) * value.num.valid, SEEK_SET);
    }

    mdl_read (seqgroup, &value, sizeof (value));
    *out += value.value * scale;
}

static void decomp_calcbone (
    FILE *seqgroup,
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
                seqgroup,
                frame,
                &bone_pos[i],
                bone->scale[i],
                animindex + anim->offset[i]);
        }
        
        if (anim->offset[3 + i] != 0)
        {
            decomp_calcbonevalue (
                seqgroup,
                frame,
                &bone_rot[i],
                bone->scale[3 + i],
                animindex + anim->offset[3 + i]);
        }
    }
}

void decomp_studioanim (
    FILE *seqgroup,
    FILE *smd,
    mstudiobone_t *bones,
    int numframes,
    int numbones,
    int animindex,
    const char *nodes)
{
    vec3_t *bone_pos = (vec3_t *)memalloc (numbones, sizeof (*bone_pos));
    vec3_t *bone_rot = (vec3_t *)memalloc (numbones, sizeof (*bone_rot));

    qc_write (smd, "version 1");
    qc_write (smd, nodes);

    int i, j;
    mstudioanim_t anim;
    
    qc_write (smd, "skeleton");

    for (i = 0; i < numframes; ++i)
    {
        qc_writef (smd, "  time %i", i);

        for (j = 0; j < numbones; ++j)
        {
            mdl_seek (seqgroup, animindex + sizeof (anim) * j, SEEK_SET);
            mdl_read (seqgroup, &anim, sizeof (anim));

            decomp_calcbone (
                seqgroup,
                i,
                bones + j,
                &anim,
                bone_pos[j],
                bone_rot[j],
                animindex + sizeof (anim) * j);
            
            if (bones[j].parent == -1)
            {
                float save = bone_pos[j][0];
                bone_pos[j][0] = bone_pos[j][1];
                bone_pos[j][1] = -save;

                bone_rot[j][2] -= Q_PI / 2.0F;
            }

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
