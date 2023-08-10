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

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include "decompile.h"
#include "studio.h"

static void decomp_bonetransform (
    studiohdr_t *header,
    mstudiobone_t *bones,
    mat4x3_t *bone_transform)
{
    int i;
    vec4_t bone_quat;
    mat4x3_t bone_matrix;

    for (i = 0; i < header->numbones; ++i)
    {
        anglequaternion (bones[i].value + 3, bone_quat);
        quaternionmatrix (bone_quat, bone_matrix);

        bone_matrix[0][3] = bones[i].value[0];
        bone_matrix[1][3] = bones[i].value[1];
        bone_matrix[2][3] = bones[i].value[2];

        if (bones[i].parent < 0)
        {
            memcpy (bone_transform[i], bone_matrix, sizeof (bone_matrix));
        }
        else
        {
            concattransforms (bone_transform[bones[i].parent], bone_matrix, bone_transform[i]);
        }
    }
}

static void decomp_writeskeleton (
    FILE *mdl,
    FILE *smd,
    studiohdr_t *header,
    mstudiobone_t *bones,
    int time)
{
    qc_writef (smd, "  time %i", time);

    int i;

    for (i = 0; i < header->numbones; ++i)
    {
        qc_writef (
            smd,
            "    %i %.06f %.06f %.06f %.06f %.06f %.06f",
            i,
            bone_print(bones[i].value));
    }
}

static void decomp_writevert (
    FILE *smd,
    byte *vert_bones,
    byte *norm_bones,
    vec3_t *verts,
    vec3_t *norms,
    float s,
    float t,
    short *cmd,
    mat4x3_t *bone_transform)
{
    byte vert_bone = vert_bones[cmd[0]];
    byte norm_bone = norm_bones[cmd[1]];
    vec3_t vert;
    vec3_t norm;

    vectortransform (verts[cmd[0]], bone_transform[vert_bone], vert);
    vectorrotate (norms[cmd[1]], bone_transform[norm_bone], norm);

    s = cmd[2] * s;
    t = 1.0F - cmd[3] * t;
    
    qc_writef (
        smd,
        "    %i %.04f %.04f %.04f %.04f %.04f %.04f %.04f %.04f",
        vert_bone,
        vec3_print (vert),
        vec3_print (norm),
        s,
        t);
}

static void decomp_mesh (
    FILE *mdl,
    FILE *smd,
    vec3_t *verts,
    vec3_t *norms,
    byte *vert_bones,
    byte *norm_bones,
    studiohdr_t *header,
    mstudiomesh_t *mesh,
    mstudiotexture_t *texture,
    mstudiobone_t *bones,
    mat4x3_t *bone_transform)
{
    float s = 1.0F / texture->width;
    float t = 1.0F / texture->height;
    
    short c;
    short cmd1[4], cmd2[4], cmd3[4];
    bool flip;

    while (true)
    {
        mdl_read (mdl, &c, sizeof (c));
        
        if (c == 0)
            break;

        mdl_read (mdl, cmd1, sizeof (cmd1));
        mdl_read (mdl, cmd2, sizeof (cmd1));
        mdl_read (mdl, cmd3, sizeof (cmd1));

        if (c < 0) /* Triangle fan */
        {
            for (c = -c - 2; c > 0; c--)
            {
                qc_writef (smd, "  %s.bmp", skippath (texture->name));

                decomp_writevert (
                    smd, vert_bones, norm_bones,
                    verts, norms, s, t, cmd1,
                    bone_transform);
                
                decomp_writevert (
                    smd, vert_bones, norm_bones,
                    verts, norms, s, t, cmd3,
                    bone_transform);
                
                decomp_writevert (
                    smd, vert_bones, norm_bones,
                    verts, norms, s, t, cmd2,
                    bone_transform);
                
                memcpy (cmd2, cmd3, sizeof (cmd1));
                
                if (c > 1)
                    mdl_read (mdl, cmd3, sizeof (cmd1));
            }
        }
        else /* Triangle strip */
        {
            flip = false;
            for (c -= 2; c > 0; c--)
            {
                qc_writef (smd, "  %s.bmp", skippath (texture->name));
                
                flip = !flip;

                decomp_writevert (
                    smd, vert_bones, norm_bones,
                    verts, norms, s, t, flip ? cmd2 : cmd1,
                    bone_transform);
                
                decomp_writevert (
                    smd, vert_bones, norm_bones,
                    verts, norms, s, t, flip ? cmd1 : cmd2,
                    bone_transform);
                
                decomp_writevert (
                    smd, vert_bones, norm_bones,
                    verts, norms, s, t, cmd3,
                    bone_transform);
                
                memcpy (cmd1, cmd2, sizeof (cmd1));
                memcpy (cmd2, cmd3, sizeof (cmd1));

                if (c > 1)
                    mdl_read (mdl, cmd3, sizeof (cmd1));
            }
        }
    }
}

static void decomp_meshes (
    FILE *mdl,
    FILE *smd,
    studiohdr_t *header,
    mstudiomodel_t *model,
    mstudiobone_t *bones,
    mat4x3_t *bone_transform)
{
    int i;
    mstudiomesh_t mesh;
    mstudiotexture_t texture;
    short skin;

    vec3_t *verts = (vec3_t *)calloc (model->numverts, sizeof (*verts));
    vec3_t *norms = (vec3_t *)calloc (model->numnorms, sizeof (*norms));
    byte *vert_bones = (byte *)calloc (model->numverts, 1);
    byte *norm_bones = (byte *)calloc (model->numnorms, 1);

    mdl_seek (mdl, model->vertindex, SEEK_SET);
    mdl_read (mdl, verts, model->numverts * sizeof (*verts));

    mdl_seek (mdl, model->normindex, SEEK_SET);
    mdl_read (mdl, norms, model->numnorms * sizeof (*norms));

    mdl_seek (mdl, model->vertinfoindex, SEEK_SET);
    mdl_read (mdl, vert_bones, model->numverts);

    mdl_seek (mdl, model->norminfoindex, SEEK_SET);
    mdl_read (mdl, norm_bones, model->numnorms);

    qc_write (smd, "triangles");

    for (i = 0; i < model->nummesh; ++i)
    {
        mdl_seek (mdl, model->meshindex + sizeof (mesh) * i, SEEK_SET);
        mdl_read (mdl, &mesh, sizeof (mesh));

        /* fprintf (stdout, "Mesh %i of %s has %i triangles\n", i, model->name, mesh.numtris); */

        mdl_seek (mdl, header->skinindex + sizeof (skin) * mesh.skinref, SEEK_SET);
        mdl_read (mdl, &skin, sizeof (skin));

        mdl_seek (mdl, header->textureindex + sizeof (texture) * skin, SEEK_SET);
        mdl_read (mdl, &texture, sizeof (texture));

        fixpath (texture.name, true);
        stripext (texture.name);

        mdl_seek (mdl, mesh.triindex, SEEK_SET);

        decomp_mesh (
            mdl,
            smd,
            verts,
            norms,
            vert_bones,
            norm_bones,
            header,
            &mesh,
            &texture,
            bones,
            bone_transform);
    }
    
    qc_write (smd, "end");

    free (norm_bones);
    free (vert_bones);
    free (norms);
    free (verts);
} 

void decomp_studiomodel (
    FILE *mdl,
    const char *smddir,
    studiohdr_t *header,
    mstudiomodel_t *model,
    const char *nodes)
{
    FILE *smd = qc_open (smddir, model->name, "smd");

    mstudiobone_t* bones = (mstudiobone_t *)calloc (header->numbones, sizeof (*bones));
    mat4x3_t *bone_transform = (mat4x3_t *)calloc (header->numbones, sizeof (*bone_transform));

    qc_write (smd, "version 1");
    qc_write (smd, nodes);

    mdl_seek (mdl, header->boneindex, SEEK_SET);
    mdl_read (mdl, bones, header->numbones * sizeof (*bones));

    decomp_bonetransform (header, bones, bone_transform);
    
    qc_write (smd, "skeleton");
    decomp_writeskeleton (mdl, smd, header, bones, 0);
    qc_write (smd, "end");

    decomp_meshes (mdl, smd, header, model, bones, bone_transform);
    
    free (bone_transform);
    free (bones);
    fclose (smd);

    fprintf (stdout, "Done!\n");
}
