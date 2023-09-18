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

void decomp_studiomodel (
    FILE *mdl,
    FILE *tex,
    const char *smddir,
    studiohdr_t *header,
    studiohdr_t *textureheader,
    mstudiomodel_t *model,
    const char *nodes);

void decomp_studiotexture (
    FILE *tex,
    const char *bmpdir,
    mstudiotexture_t *texture);

void decomp_studioanim (
    FILE *seqgroup,
    FILE *smd,
    mstudiobone_t *bones,
    int numframes,
    int numbones,
    int animindex,
    const char *nodes);

static void decomp_writeinfo (
    FILE *mdl,
    FILE *tex,
    FILE *qc,
    const char *cd,
    const char *cdtexture,
    studiohdr_t *header,
    studiohdr_t *textureheader)
{
    qc_write (qc, "/*");
    qc_write (qc, "==============================================================================");
    qc_writef (qc, "%s", skippath (header->name));
    qc_write (qc, "==============================================================================");
    qc_write (qc, "*/");
    qc_putc (qc, '\n');

    /* Toodles: Keep the original output name because GoldSrc's filesystem is case-sensitive. */

    qc_writef (qc, "$modelname %s", header->name);
    qc_writef (qc, "$cd %s", cd);
    qc_writef (qc, "$cdtexture %s", cdtexture);
    
    if (header->numtextures == 0)
    {
        qc_write (qc, "$externaltextures");
    }

    qc_putc (qc, '\n');

    int i;
    mstudiotexture_t texture;
    bool wrote = false;

    mdl_seek (tex, textureheader->textureindex, SEEK_SET);
    for (i = 0; i < textureheader->numtextures; ++i)
    {
        mdl_read (tex, &texture, sizeof (texture));

        if (!(texture.flags & ~(STUDIO_NF_CHROME | STUDIO_NF_FLATSHADE)))
            continue;

        fixpath (texture.name, true);
        stripext (texture.name);
        
        if (texture.flags & STUDIO_NF_ADDITIVE)
        {
            qc_writef (qc, "$texrendermode %s.bmp additive", texture.name);
            wrote = true;
        }
        if (texture.flags & STUDIO_NF_MASKED)
        {
            qc_writef (qc, "$texrendermode %s.bmp masked", texture.name);
            wrote = true;
        }
        /* Toodles: Sven Co-op & Xash3D support this. */
        if (texture.flags & STUDIO_NF_FULLBRIGHT)
        {
            qc_writef (qc, "$texrendermode %s.bmp fullbright", texture.name);
            wrote = true;
        }
    }
    
    if (wrote)
    {
        qc_putc (qc, '\n');
    }

    if (vec3_nonzero (header->eyeposition))
    {
        qc_writef (qc, "$eyeposition %g %g %g", vec3_print (header->eyeposition));
        qc_putc (qc, '\n');
    }
}

static char *decomp_makenodes (FILE *mdl, studiohdr_t *header)
{
    char line[45];
    mstudiobone_t bone;
    
    char *str = (char *)memalloc (10 + 45 * header->numbones, 1);

    mdl_seek (mdl, header->boneindex, SEEK_SET);
    strcat (str, "nodes\n");

    int i;
    for (i = 0; i < header->numbones; ++i)
    {
        mdl_read (mdl, &bone, sizeof (bone));
        sprintf (line, "  %i \"%s\" %i\n", i, bone.name, bone.parent);
        strcat (str, line);
    }

    strcat (str, "end");

    return str;
}

static void decomp_writebodygroups (
    FILE *mdl,
    FILE *tex,
    FILE *qc,
    const char *smddir,
    studiohdr_t *header,
    studiohdr_t *textureheader,
    const char *nodes)
{
    int i, j;
    mstudiobodyparts_t bodypart;
    mstudiomodel_t model;
    bool group;

    for (i = 0; i < header->numbodyparts; ++i)
    {
        mdl_seek (mdl, header->bodypartindex + sizeof (bodypart) * i, SEEK_SET);
        mdl_read (mdl, &bodypart, sizeof (bodypart));

        group = bodypart.nummodels > 1;

        if (group)
        {
            qc_writef (qc, "$bodygroup %s", bodypart.name);
            qc_putc (qc, '{');
            qc_putc (qc, '\n');
        }

        for (j = 0; j < bodypart.nummodels; ++j)
        {
            mdl_seek (mdl, bodypart.modelindex + sizeof (model) * j, SEEK_SET);
            mdl_read (mdl, &model, sizeof(model));

            if (!strcasecmp (model.name, "blank") || strlen (model.name) == 0)
            {
                qc_write (qc, group ? "    blank" : "blank");
                continue;
            }

            fixpath (model.name, true);
            
            qc_writef (
                qc,
                group ? "    studio \"%s\"" : "$body studio \"%s\"",
                model.name);

            decomp_studiomodel (mdl, tex, smddir, header, textureheader, &model, nodes);
        }

        if (group)
        {
            qc_putc (qc, '}');
            qc_putc (qc, '\n');
        }
    }

    qc_putc (qc, '\n');
}

typedef struct {
    int first;
    int length;
    void *next;
    char channels[];
} texturegroup_t;

static void decomp_writeskingroups (
    FILE *mdl,
    FILE *tex,
    FILE *qc,
    studiohdr_t *header,
    studiohdr_t *textureheader)
{
    mstudiobodyparts_t bodypart;
    mstudiomodel_t model;
    mstudiomesh_t mesh;
    char texture_name[64];
    int i, j, k, l;

    int numtexturegroups = 0;
    bool foundgroup;
    texturegroup_t texturegroup;
    texturegroup_t *texturegroups = NULL;
    texturegroup_t *currentgroup;
    texturegroup_t *newgroup;

    short *skins = (short *)memalloc (textureheader->numskinfamilies * textureheader->numskinref, sizeof (*skins));
    short *start, *end, *cur;

    mdl_seek (tex, textureheader->skinindex, SEEK_SET);
    mdl_read (tex, skins, textureheader->numskinfamilies * textureheader->numskinref * sizeof (*skins));

    for (i = 0; i < header->numbodyparts; ++i)
    {
        mdl_seek (mdl, header->bodypartindex + sizeof (bodypart) * i, SEEK_SET);
        mdl_read (mdl, &bodypart, sizeof (bodypart));

        for (j = 0; j < bodypart.nummodels; ++j)
        {
            mdl_seek (mdl, bodypart.modelindex + sizeof (model) * j, SEEK_SET);
            mdl_read (mdl, &model, sizeof(model));

            if (!strcasecmp (model.name, "blank") || strlen (model.name) == 0)
                continue;

            for (k = 0; k < model.nummesh; ++k)
            {
                mdl_seek (mdl, model.meshindex + sizeof (mesh) * k, SEEK_SET);
                mdl_read (mdl, &mesh, sizeof (mesh));

                start = end = cur = skins + mesh.skinref;

                texturegroup.first = 0;
                texturegroup.length = 1;

                for (l = 1; l < textureheader->numskinfamilies; ++l)
                {
                    cur += textureheader->numskinref;

                    if (*cur != *start)
                    {
                        end = cur;
                        texturegroup.length++;
                    }
                    else
                    {
                        start = end = cur;
                        texturegroup.first = l;
                        texturegroup.length = 1;
                    }
                }

                if (start == end)
                    continue;

                foundgroup = false;
                currentgroup = texturegroups;

                while (currentgroup != NULL)
                {
                    if (texturegroup.first == currentgroup->first
                        && texturegroup.length == currentgroup->length)
                    {
                        currentgroup->channels[mesh.skinref] = true;
                        foundgroup = true;
                        break;
                    }
                    currentgroup = currentgroup->next;
                }

                if (!foundgroup)
                {
                    newgroup = (texturegroup_t *)memalloc (1, sizeof (*newgroup) + textureheader->numskinref);
                    memcpy (newgroup, &texturegroup, sizeof (texturegroup));
                    newgroup->channels[mesh.skinref] = true;
                    newgroup->next = texturegroups;
                    texturegroups = newgroup;
                    numtexturegroups++;
                }
            }
        }
    }

    if (numtexturegroups == 0)
        goto skins_done;
    
    numtexturegroups = 0;
    currentgroup = texturegroups;

    while (currentgroup != NULL)
    {
        numtexturegroups++;
        qc_writef (qc, "$texturegroup group%i", numtexturegroups);
        qc_putc (qc, '{');
        qc_putc (qc, '\n');

        cur = skins + textureheader->numskinref * currentgroup->first;

        for (i = currentgroup->first; i < currentgroup->first + currentgroup->length; ++i)
        {
            qc_write2f (qc, "    { ");

            for (j = 0; j < textureheader->numskinref; ++j)
            {
                if (!currentgroup->channels[j])
                    continue;

                mdl_seek (tex, textureheader->textureindex + sizeof (mstudiotexture_t) * cur[j], SEEK_SET);
                mdl_read (tex, &texture_name, sizeof (texture_name));

                fixpath (texture_name, true);
                stripext (texture_name);

                qc_write2f (qc, "\"%s.bmp\" ", skippath (texture_name));
            }
            
            qc_putc (qc, '}');
            qc_putc (qc, '\n');

            cur += textureheader->numskinref;
        }

        qc_putc (qc, '}');
        qc_putc (qc, '\n');

        newgroup = currentgroup->next;
        free (currentgroup);
        currentgroup = newgroup;
    }
    qc_putc (qc, '\n');

skins_done:
    free (skins);
}

static void decomp_writeattachments (FILE *mdl, FILE *qc, studiohdr_t *header)
{
    if (header->numattachments <= 0)
        return;
    
    int i;

    mstudioattachment_t attachment;
    char bone_name[32];

    for (i = 0; i < header->numattachments; ++i)
    {
        mdl_seek (mdl, header->attachmentindex + sizeof (attachment) * i, SEEK_SET);
        mdl_read (mdl, &attachment, sizeof (attachment));

        mdl_seek (mdl, header->boneindex + sizeof (mstudiobone_t) * attachment.bone, SEEK_SET);
        mdl_read (mdl, bone_name, sizeof (bone_name));
        
        qc_writef (qc, "$attachment %i \"%s\" %g %g %g", i, bone_name, vec3_print(attachment.org));
    }
    
    qc_putc (qc, '\n');
}

static void decomp_writecontrollers (FILE *mdl, FILE *qc, studiohdr_t *header)
{
    if (header->numbonecontrollers <= 0)
        return;
    
    int i;

    mstudiobonecontroller_t ctrl;
    char bone_name[32];

    for (i = 0; i < header->numbonecontrollers; ++i)
    {
        mdl_seek (mdl, header->bonecontrollerindex + sizeof (ctrl) * i, SEEK_SET);
        mdl_read (mdl, &ctrl, sizeof (ctrl));

        mdl_seek (mdl, header->boneindex + sizeof (mstudiobone_t) * ctrl.bone, SEEK_SET);
        mdl_read (mdl, bone_name, sizeof (bone_name));

        qc_write2f (qc, "$controller ");

        if (ctrl.index == 4)
        {
            qc_write2f (qc, "mouth ");
        }
        else
        {
            qc_write2f (qc, "%i ", ctrl.index);
        }

        qc_writef (
            qc,
            "\"%s\" %s %g %g",
            bone_name,
            mdl_getmotionflag (ctrl.type),
            ctrl.start,
            ctrl.end);
    }
    
    qc_putc (qc, '\n');
}

static void decomp_writehitboxes (FILE *mdl, FILE *qc, studiohdr_t *header)
{
    if (header->numhitboxes <= 0)
        return;
    
    int i;

    mstudiobbox_t hbox;
    char bone_name[32];
    bool has_hbox = false;

    for (i = 0; i < header->numhitboxes; ++i)
    {
        mdl_seek (mdl, header->hitboxindex + sizeof (hbox) * i, SEEK_SET);
        mdl_read (mdl, &hbox, sizeof (hbox));

        /* Toodles TODO: This skips auto generated boxes. It might not be a good idea. */
        if (hbox.group == 0)
            continue;
        
        has_hbox = true;

        mdl_seek (mdl, header->boneindex + sizeof (mstudiobone_t) * hbox.bone, SEEK_SET);
        mdl_read (mdl, bone_name, sizeof (bone_name));

        qc_writef (
            qc,
            "$hbox %i \"%s\" %g %g %g  %g %g %g",
            hbox.group, bone_name,
            vec3_print(hbox.bbmin),
            vec3_print(hbox.bbmax));
    }

    if (has_hbox)
        qc_putc (qc, '\n');
}

static bool decomp_simplesequence (mstudioseqdesc_t *seq)
{
    return seq->numevents <= 0
        && seq->numblends <= 1
        && seq->activity <= 0
        && seq->numpivots <= 0
        && seq->blendtype[0] == 0
        && seq->blendtype[1] == 0
        && seq->entrynode == 0
        && seq->exitnode == 0
        && seq->nodeflags == 0
        && (seq->motiontype & STUDIO_TYPES) == 0;
}

static void decomp_writeseqblends (FILE *qc, const char *cdanim, mstudioseqdesc_t *seq)
{
    if (seq->numblends <= 1)
    {
        qc_writef (qc, "    \"%s/%s\"", cdanim, seq->label);
    }
    else if (seq->numblends == 2)
    {
        /* I'm certain this is always the case with official models. */
        qc_writef (qc, "    \"%s/%s_down\"", cdanim, seq->label);
        qc_writef (qc, "    \"%s/%s_up\"", cdanim, seq->label);
    }
    else
    {
        int i;
        for (i = 0; i < seq->numblends; ++i)
        {
            qc_writef (qc, "    \"%s/%s_blend_%.02i\"", cdanim, seq->label, i);
        }
    }

    qc_writef (qc, "    fps %g", seq->fps);
    
    if (seq->flags & STUDIO_LOOPING)
    {
        qc_write (qc, "    loop");
    }
}

static void decomp_writeseqact (FILE *qc, mstudioseqdesc_t *seq)
{
    if (seq->activity == 0)
        return;

    qc_write2f (qc, "    %s", mdl_getactname (seq->activity));

    if (seq->actweight != 0)
    {
        qc_write2f (qc, " %i", seq->actweight);
    }

    qc_putc (qc, '\n');
}

static void decomp_writeseqpivots (FILE *mdl, FILE *qc, mstudioseqdesc_t *seq)
{
    /* Toodles: This seems to be an unfinished feature. Keeping it because StudioMDL does write it. */
    if (seq->numpivots <= 0)
        return;

    int i;
    mstudiopivot_t pivot;
    
    mdl_seek (mdl, seq->pivotindex, SEEK_SET);
    
    for (i = 0; i < seq->numpivots; ++i)
    {
        mdl_read (mdl, &pivot, sizeof (pivot));
        qc_writef (qc, "    pivot %i %i %i", i, pivot.start, pivot.end);
    }
}

static void decomp_writeseqblendtype (FILE *qc, mstudioseqdesc_t *seq)
{
    int i;
    
    for (i = 0; i < 2; ++i)
    {
        if (seq->blendtype[i] == 0)
            continue;
        
        qc_writef (
            qc,
            "    blend %s %g %g",
            mdl_getmotionflag (seq->blendtype[i]),
            seq->blendstart[i],
            seq->blendend[i]);
    }
}

static void decomp_writeseqnodes (FILE *qc, mstudioseqdesc_t *seq)
{
    if (seq->entrynode == 0
            && seq->exitnode == 0
            && seq->nodeflags == 0)
        return;
    
    /* Toodles TODO: I haven't tested this. */
    if (seq->nodeflags != 0)
    {
        qc_writef (qc, "    rtransition %i %i", seq->entrynode, seq->exitnode);
    }
    else if (seq->entrynode == seq->exitnode)
    {
        qc_writef (qc, "    node %i", seq->entrynode);
    }
    else
    {
        qc_writef (qc, "    transition %i %i", seq->entrynode, seq->exitnode);
    }
}

static void decomp_writeseqmotiontype (FILE *qc, mstudioseqdesc_t *seq)
{
    if (!(seq->motiontype & STUDIO_TYPES))
        return;

    int i;

    qc_write2f (qc, "   ");
    
    for (i = 0; i < 15; ++i)
    {
        if ((seq->motiontype & STUDIO_TYPES) & (1 << i))
        {
            qc_write2f (qc, " %s", mdl_getmotionflag (1 << i));
        }
    }
    qc_putc (qc, '\n');
}

static void decomp_writeseqevents (FILE *mdl, FILE *qc, mstudioseqdesc_t *seq)
{
    if (seq->numevents <= 0)
        return;
    
    int i;
    mstudioevent_t event;

    mdl_seek (mdl, seq->eventindex, SEEK_SET);
    
    for (i = 0; i < seq->numevents; ++i)
    {
        mdl_read (mdl, &event, sizeof (event));
        qc_writef (
            qc,
            "    { event %i %i \"%s\" }",
            event.event,
            event.frame,
            event.options);
    }
}

static void decomp_writeseqdesc (
    FILE *mdl,
    FILE *qc,
    const char *cdanim,
    studiohdr_t *header,
    mstudioseqdesc_t *seq)
{
    qc_putc (qc, ' ');
    qc_putc (qc, '{');
    qc_putc (qc, '\n');
    
    decomp_writeseqblends (qc, cdanim, seq);
    decomp_writeseqact (qc, seq);
    decomp_writeseqpivots (mdl, qc, seq);
    decomp_writeseqblendtype (qc, seq);
    decomp_writeseqnodes (qc, seq);
    decomp_writeseqmotiontype (qc, seq);
    decomp_writeseqevents (mdl, qc, seq);

    qc_putc (qc, '}');
    qc_putc (qc, '\n');
}

static void decomp_writeanimations (
    FILE *mdl,
    FILE **seqgroups,
    const char *animdir,
    studiohdr_t *header,
    const char *nodes,
    mstudioseqdesc_t *seq,
    mstudiobone_t *bones)
{
    mstudioseqgroup_t group;
    char *animname = (char *)memalloc (strlen (seq->label) + 16, 1);
    char blendnum[4];
    FILE *smd;

    mdl_seek (mdl, header->seqgroupindex + sizeof (group) * seq->seqgroup, SEEK_SET);
    mdl_read (mdl, &group, sizeof (group));

    FILE *seqgroup = mdl;
    int animindex = group.unused2 + seq->animindex;

    if (seq->seqgroup > 0)
    {
        seqgroup = seqgroups[seq->seqgroup];
        animindex = seq->animindex;
    }

    int i;

    for (i = 0; i < seq->numblends; ++i)
    {
        strcpy (animname, seq->label);

        if (seq->numblends == 2)
        {
            strcat (animname, i == 0 ? "_down" : "_up");
        }
        else if (seq->numblends > 2)
        {
            strcat (animname, "_blend_");
            snprintf (blendnum, 8, "%.02i", i);
            strcat (animname, blendnum);
        }

        smd = qc_open (animdir, animname, "smd", false);

        decomp_studioanim (
            seqgroup,
            smd,
            bones,
            seq->numframes,
            header->numbones,
            animindex + sizeof (mstudioanim_t) * header->numbones * i,
            nodes);

        fclose (smd);
        fprintf (stdout, "Done!\n");
    }

    free (animname);
}

static void decomp_writesequences (
    FILE *mdl,
    FILE **seqgroups,
    FILE *qc,
    const char *smddir,
    const char *cdanim,
    studiohdr_t *header,
    const char *nodes)
{
    if (header->numseq <= 0)
        return;
    
    int i;

    mstudioseqdesc_t seq;

    char *animdir = appenddir (smddir, cdanim);
    mstudiobone_t *bones = (mstudiobone_t *)memalloc (header->numbones, sizeof (*bones));

    mdl_seek (mdl, header->boneindex, SEEK_SET);
    mdl_read (mdl, bones, header->numbones * sizeof (*bones));

    for (i = 0; i < header->numseq; ++i)
    {
        mdl_seek (mdl, header->seqindex + sizeof(seq) * i, SEEK_SET);
        mdl_read (mdl, &seq, sizeof(seq));

        qc_write2f (qc, "$sequence %s", seq.label);

        fixpath (seq.label, true);

        decomp_writeanimations (mdl, seqgroups, animdir, header, nodes, &seq, bones);

        if (!decomp_simplesequence (&seq))
        {
            decomp_writeseqdesc (mdl, qc, cdanim, header, &seq);
            continue;
        }

        qc_write2f (qc, " \"%s/%s\" fps %g", cdanim, seq.label, seq.fps);
        
        if (seq.flags & STUDIO_LOOPING)
            qc_write2f (qc, " loop");
        
        qc_putc (qc, '\n');
    }

    free (animdir);
    free (bones);
}

void decomp_writetextures (
    FILE *tex,
    const char *smddir,
    const char *cdtexture,
    studiohdr_t *textureheader)
{
    int i;
    mstudiotexture_t texture;

    char *bmpdir = appenddir (smddir, cdtexture);

    for (i = 0; i < textureheader->numtextures; ++i)
    {
        mdl_seek (tex, textureheader->textureindex + sizeof (texture) * i, SEEK_SET);
        mdl_read (tex, &texture, sizeof (texture));

        fixpath (texture.name, true);
        stripext (texture.name);
        
        decomp_studiotexture (tex, bmpdir, &texture);
    }

    free (bmpdir);
}

static void decomp_loadtextures (
    const char *mdlname,
    FILE **tex,
    studiohdr_t *textureheader)
{
    char *texname = (char *)memalloc (strlen (mdlname) + 2, 1);
    char *name, *ext;
    int id;
    int version;

    filebase (mdlname, &name, &ext);
    
    strcpy (texname, mdlname);
    stripext (texname);
    strcat (texname, "t");
    strcat (texname, ext);

#ifndef WIN32
    *tex = mdl_open (texname, &id, &version, true);
    
    if (!tex)
    {
        texname[strlen (mdlname) - strlen (ext)] = '\0';
        strcat (texname, "T");
        strcat (texname, ext);

        *tex = mdl_open (texname, &id, &version, false);
    }
#else
    *tex = mdl_open (texname, &id, &version, false);
#endif

    free (texname);

    if (id != IDSTUDIOHEADER)
        error (1, "Not a Valve MDL\n");
    
    if (version != STUDIO_VERSION)
        error (1, "Wrong MDL version: %i\n", version);

    mdl_read (*tex, textureheader, sizeof (*textureheader));
}

static void decomp_loadseqgroups (
    const char *mdlname,
    FILE ***seqgroups,
    studioseqhdr_t **seqheaders,
    int numseqgroups)
{
    int i;
    char *seqgroupname = (char *)memalloc (strlen (mdlname) + 3, 1);
    int id;
    int version;

    *seqgroups = (FILE **)memalloc (numseqgroups, sizeof (**seqgroups));
    *seqheaders = (studioseqhdr_t *)memalloc (numseqgroups, sizeof (**seqheaders));

    for (i = 1; i < numseqgroups; ++i)
    {
        strcpy (seqgroupname, mdlname);
        sprintf (&seqgroupname[strlen(seqgroupname) - 4], "%02d.mdl", i);
        
        (*seqgroups)[i] = mdl_open(seqgroupname, &id, &version, false);
        
        if (id != IDSTUDIOSEQHEADER)
            error (1, "Not a Valve MDL sequence group\n");
        
        if (version != STUDIO_VERSION)
            error (1, "Wrong MDL version: %i\n", version);

        mdl_read((*seqgroups)[i], &(*seqheaders)[i], sizeof (**seqheaders));
    }

    free (seqgroupname);
}

void decomp_mdl (
    const char *mdlname,
    const char *qcname,
    const char *cd,
    const char *cdtexture,
    const char *cdanim,
    const char *qcdir,
    const char *smddir)
{
    int id;
    int version;
    FILE *mdl = mdl_open (mdlname, &id, &version, false);

    if (id != IDSTUDIOHEADER)
        error (1, "Not a Valve MDL\n");
    
    if (version != STUDIO_VERSION)
        error (1, "Wrong MDL version: %i\n", version);
    
    FILE *qc = qc_open (qcdir, qcname, "qc", false);

    studiohdr_t header;
    mdl_read (mdl, &header, sizeof (header));
    fixpath (header.name, false);

    /* Init the texture vars with the regular model info. */
    FILE *tex = mdl;
    studiohdr_t textureheader;
    memcpy (&textureheader, &header, sizeof (textureheader));

    /* Init the texture model if necessary. */
    if (header.numtextures == 0)
    {
        decomp_loadtextures (mdlname, &tex, &textureheader);
    }

    FILE **seqgroups;
    studioseqhdr_t *seqheaders;

    if (header.numseqgroups > 1)
    {
        decomp_loadseqgroups (mdlname, &seqgroups, &seqheaders, header.numseqgroups);
    }

    char *nodes = decomp_makenodes (mdl, &header);

    decomp_writeinfo (mdl, tex, qc, cd, cdtexture, &header, &textureheader);
    decomp_writebodygroups (mdl, tex, qc, smddir, &header, &textureheader, nodes);
    decomp_writeskingroups (mdl, tex, qc, &header, &textureheader);
    decomp_writeattachments (mdl, qc, &header);
    decomp_writecontrollers (mdl, qc, &header);
    decomp_writehitboxes (mdl, qc, &header);
    decomp_writesequences (mdl, seqgroups, qc, smddir, cdanim, &header, nodes);
    decomp_writetextures (tex, smddir, cdtexture, &textureheader);

    free (nodes);

    if (header.numseqgroups > 1)
    {
        int i;
        for (i = 1; i < header.numseqgroups; ++i)
        {
            fclose (seqgroups[i]);
            fprintf (stdout, "Done!\n");
        }
        free (seqheaders);
        free (seqgroups);
    }

    if (header.numtextures == 0)
    {
        fclose (tex);
        fprintf (stdout, "Done!\n");
    }
    
    fclose (qc);
    fclose (mdl);

    fprintf (stdout, "Done!\n");
}
