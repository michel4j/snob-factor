#define NOTGLOB 1
#define SAMPLES 1
#include "glob.h"

int qssamp();

/*	----------------------  printdatum  -------------------------  */
/*	To print datum for variable i, case n, in sample     */
void printdatum(int i, int n) {
    Sample *samp;
    AVinst *avi;
    SVinst *svi;
    Vtype *vtp;

    samp = ctx.sample;
    svi = samp->svars + i;
    avi = vlist + i;
    vtp = avi->vtp;
    loc = (char *)samp->recs;
    loc += n * samp->reclen;
    loc += svi->offset;
    /*	Test for missing  */
    if (*loc == 1) {
        printf("    =====");
        return;
    }
    loc += 1;
    (*vtp->printdat)(loc);
    return;
}
/*	------------------------ readvset ------------------------------ */
/*	To read in a vset from a file. Returns index of vset  */
/*	Returns negative if fail*/
int readvset() {

    int i, itype, indx;
    int kread;
    Buf bufst, *buf;
    char *vaux;

    buf = &bufst;
    for (i = 0; i < Maxvsets; i++)
        if (!vsets[i])
            goto gotit;
nospce:
    printf("No space for VariableSet\n");
    i = -10;
    goto error;

gotit:
    indx = i;
    vst = vsets[i] = (Vset *)malloc(sizeof(Vset));
    if (!vst)
        goto nospce;
    vst->id = indx;
    ctx.vset = vst;
    vst->vlist = 0;
    vst->vblks = 0;
    printf("Enter variable-set file name:\n");
    kread = readalf(vst->dfname, 1);
    strcpy(buf->cname, vst->dfname);
    ctx.buffer = buf;
    if (bufopen()) {
        printf("Cant open variable-set file %s\n", vst->dfname);
        i = -2;
        goto error;
    }

    /*	Begin to read variable-set file. First entry is its name   */
    newline();
    kread = readalf(vst->name, 1);
    if (kread < 0) {
        printf("Error in name of variable-set\n");
        i = -9;
        goto error;
    }
    /*	Num of variables   */
    newline();
    kread = readint(&nv, 1);
    if (kread) {
        printf("Cant read number of variables\n");
        i = -4;
        goto error;
    }
    vst->nv = nv;
    vst->nactv = vst->nv;

    /*	Make a vec of nv AVinst blocks  */
    vlist = (AVinst *)gtsp(3, nv * sizeof(AVinst));
    if (!vlist) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    vst->vlist = vlist;

    /*	Read in the info for each variable into vlist   */
    for (i = 0; i < nv; i++) {
        vlist[i].id = -1;
        vlist[i].vaux = 0;
    }
    for (i = 0; i < nv; i++) {
        avi = vlist + i;
        avi->id = i;

        /*	Read name  */
        newline();
        kread = readalf(avi->name, 1);
        if (kread < 0) {
            printf("Error in name of variable %d\n", i + 1);
            i = -10;
            goto error;
        }

        /*	Read type code. Negative means idle.   */
        kread = readint(&itype, 1);
        if (kread) {
            printf("Cant read type code for var %d\n", i + 1);
            i = -5;
            goto error;
        }
        avi->idle = 0;
        if (itype < 0) {
            avi->idle = 1;
            itype = -itype;
        }
        if ((itype < 1) || (itype > Ntypes)) {
            printf("Bad type code %d for var %d\n", itype, i + 1);
            i = -5;
            goto error;
        }
        itype = itype - 1; /*  Convert types to start at 0  */
        vtp = avi->vtp = types + itype;
        avi->itype = itype;

        /*	Make the vaux block  */
        vaux = (char *)gtsp(3, vtp->vauxsize);
        if (!vaux) {
            printf("Cant make auxilliary var block\n");
            i = -6;
            goto error;
        }
        avi->vaux = vaux;

        /*	Read auxilliary information   */
        if ((*vtp->readvaux)(vaux)) {
            printf("Error in reading auxilliary info var %d\n", i + 1);
            i = -7;
            goto error;
        }
        /*	Set sizes of stats and basic blocks for var in classes */
        (*vtp->setsizes)(i);
    } /* End of variables loop */
    i = indx;

error:
    bufclose();
    ctx.buffer = source;
    return (i);
}

/*	---------------------  readsample -------------------------  */
/*	To open a sample file and read in all about the sample.
    Returns index of new sample in samples array   */
int readsample(char *fname) {

    int i, n;
    int kread;
    int caseid;
    Buf bufst, *buf;
    Context oldctx;
    char *saux, vstnam[80], sampname[80];

    cmcpy(&oldctx, &ctx, sizeof(Context));
    buf = &bufst;
    strcpy(buf->cname, fname);
    ctx.buffer = buf;
    if (bufopen()) {
        printf("Cannot open sample file %s\n", buf->cname);
        i = -2;
        goto error;
    }

    /*	Begin to read sample file. First entry is its name   */
    newline();
    kread = readalf(sampname, 1);
    if (kread < 0) {
        printf("Error in name of sample\n");
        i = -9;
        goto error;
    }
    /*	See if sample already loaded  */
    if (sname2id(sampname, 0) >= 0) {
        printf("Sample %s already present\n", sampname);
        i = -8;
        goto error;
    }
    /*	Next line should be the vset name  */
    newline();
    kread = readalf(vstnam, 1);
    if (kread < 0) {
        printf("Error in name of variableset\n");
        i = -9;
        goto error;
    }
    /*	Check vset known  */
    kread = vname2id(vstnam);
    if (kread < 0) {
        printf("Variableset %s unknown\n", vstnam);
        i = -8;
        goto error;
    }
    vst = ctx.vset = vsets[kread];

    /*	Find a vacant sample slot  */
    for (i = 0; i < Maxsamples; i++) {
        if (samples[i] == 0)
            goto gotit;
    }
    goto nospace;

gotit:
    samp = samples[i] = (Sample *)malloc(sizeof(Sample));
    if (!samp)
        goto nospace;
    ctx.sample = samp;
    samp->sblks = 0;
    samp->id = i;
    strcpy(samp->dfname, buf->cname);
    strcpy(samp->name, sampname);
    /*	Set variable-set name in sample  */
    strcpy(samp->vstname, vst->name);
    /*	Num of variables   */
    nv = vst->nv;
    vlist = vst->vlist;

    /*	Make a vec of nv SVinst blocks  */
    svars = (SVinst *)gtsp(0, nv * sizeof(SVinst));
    if (!svars) {
        printf("Cannot allocate memory for variables blocks\n");
        i = -3;
        goto error;
    }
    samp->svars = svars;

    /*	Read in the info for each variable into svars   */
    for (i = 0; i < nv; i++) {
        svars[i].id = -1;
        svars[i].saux = 0;
        svars[i].offset = 0;
        svars[i].nval = 0;
    }
    reclen = 1 + sizeof(int); /* active flag and ident  */
    for (i = 0; i < nv; i++) {
        svi = svars + i;
        svi->id = i;
        avi = vlist + i;
        vtp = avi->vtp;

        /*	Make the saux block  */
        saux = (char *)gtsp(0, vtp->sauxsize);
        if (!saux) {
            printf("Cant make auxilliary var block\n");
            i = -6;
            goto error;
        }
        svi->saux = saux;

        /*	Read auxilliary information   */
        if ((*vtp->readsaux)(saux)) {
            printf("Error in reading auxilliary info var %d\n", i + 1);
            i = -7;
            goto error;
        }

        /*	Set the offset of the (missing, value) pair  */
        svi->offset = reclen;
        reclen += (1 + vtp->datsize); /* missing flag and value */
    }                                 /* End of variables loop */

    /*	Now attempt to read in the data. The first item is the number of cases*/
    newline();
    kread = readint(&nc, 1);
    if (kread) {
        printf("Cant read number of cases\n");
        i = -11;
        goto error;
    }
    samp->nc = nc;
    samp->nact = 0;
    /*	Make a vector of nc records each of size reclen  */
    recs = samp->recs = loc = (char *)gtsp(0, nc * reclen);
    if (!loc) {
        printf("No space for data\n");
        i = -8;
        goto error;
    }
    samp->reclen = reclen;

    /*	Read in the data cases, each preceded by an active flag and ident   */
    for (n = 0; n < nc; n++) {
        newline();
        kread = readint(&caseid, 1);
        if (kread) {
            printf("Cant read ident, case %d\n", n + 1);
            i = -12;
            goto error;
        }
        /*	If ident negative, so clear active */
        if (caseid < 0) {
            caseid = -caseid;
            *loc = 0;
        } else {
            *loc = 1;
            samp->nact++;
        }
        loc++;
        cmcpy(loc, &caseid, sizeof(int));
        loc += sizeof(int);
        /*	Posn now points to where the (missing, val) pair for the
        attribute should start.  */
        for (i = 0; i < nv; i++) {
            svi = svars + i;
            avi = vlist + i;
            vtp = avi->vtp;
            kread = (*vtp->readdat)(loc + 1, i);
            if (kread < 0) {
                printf("Data error case %d var %d\n", n + 1, i + 1);
                swallow();
            }
            if (kread)
                *loc = 1; /* Data missing */
            else {
                *loc = 0;
                svi->nval++;
            }
            loc += (vtp->datsize + 1);
        }
    }
    printf("Number of active cases = %d\n", samp->nact);
    bufclose();
    ctx.buffer = source;
    if (qssamp(samp)) {
        printf("Sort failure on sample\n");
        return (-1);
    }
    return (samp->id);

nospace:
    printf("No space for another sample\n");
    i = -1;
error:
    bufclose();
    cmcpy(&ctx, &oldctx, sizeof(Context));
    return (i);
}

/*	-----------------------  sname2id  ------------------------  */
/*	To find sample id given its name. Returns -1 if unknown  */
/*	'expect' shows if sample expected to be present  */
int sname2id(nam, expect)
char *nam;
int expect;
{
    int i;

    for (i = 0; i < Maxsamples; i++) {
        if (samples[i]) {
            if (!strcmp(nam, samples[i]->name))
                goto searched;
        }
    }
    i = -1;

searched:
    if ((i < 0) && expect)
        printf("Cannot find sample %s\n", nam);
    if ((i >= 0) && (!expect))
        printf("Sample %s already loaded\n", nam);
    return (i);
}

/*	-----------------------  vname2id  ------------------------  */
/*	To find vset id given its name. Returns -1 if unknown  */
int vname2id(nam)
char *nam;
{
    int i, ii;

    ii = -1;
    for (i = 0; i < Maxvsets; i++) {
        if (vsets[i]) {
            if (!strcmp(nam, vsets[i]->name))
                ii = i;
        }
    }
    if (ii < 0)
        printf("Cannot find variable set %s\n", nam);
    return (ii);
}

/*	------------------  qssamp  ----------------------------  */
/*	To quicksort a sample into increasing ident order   */

/*	Record swapper  */
void swaprec(p1, p2, ll) char *p1, *p2;
int ll;
{
    int tt;
    if (p1 == p2)
        return;
    while (ll) {
        tt = *p1;
        *p1 = *p2;
        *p2 = tt;
        p1++;
        p2++;
        ll--;
    }
    return;
}

/*	Recursive quicksort  */
void qssamp1(bot, nn, len) char *bot;
int nn, len;
{
    char *top, *rp1, *rp2, *cen;
    int av, bv, cv, nb, nt;

again:
    if (nn < 2)
        return;
    if (nn >= 6)
        goto recurse;
    /*	Do a short block by bubble  */
    rp1 = bot;
    for (nt = 0; nt < nn - 1; nt++) {
        cmcpy(&bv, rp1 + 1, sizeof(int));
        rp2 = cen = rp1;
        for (nb = nt + 1; nb < nn; nb++) {
            rp2 += len;
            cmcpy(&av, rp2 + 1, sizeof(int));
            if (av < bv) {
                bv = av;
                cen = rp2;
            }
        }
        if (cen != rp1)
            swaprec(cen, rp1, len);
        rp1 += len;
    }
    return;

recurse:
    /*	Pick a random central value  */
    nt = nn * fran();
    if (nt == nn)
        nt = nn / 2;
    cen = bot + nt * len;
    cmcpy(&cv, cen + 1, sizeof(int));
    top = bot + (nn - 1) * len;
    rp1 = bot;
    rp2 = top;
    nt = nb = 0;
loop1:
    cmcpy(&av, rp2 + 1, sizeof(int));
    if (av >= cv) {
        nt++;
        rp2 -= len;
        if (rp2 >= rp1)
            goto loop1;
        else
            goto done;
    }
loop2:
    cmcpy(&bv, rp1 + 1, sizeof(int));
    if (bv < cv) {
        nb++;
        rp1 += len;
        if (rp2 >= rp1)
            goto loop2;
        else
            goto done;
    }
    /*	Have av < cv, bv >= cv  */
    swaprec(rp1, rp2, len);
    nt++;
    rp2 -= len;
    nb++;
    rp1 += len;
    if (rp2 >= rp1)
        goto loop1;

done:
    /*	Check that something has been placed in lower block.  */
    if (nb)
        goto doboth;
    /*	Nothing was less than cv, the value at cen, so swap it to bot */
    swaprec(cen, bot, len);
    nn--;
    bot += len;
    goto again;

doboth:
    qssamp1(bot, nb, len);
    qssamp1(bot + nb * len, nt, len);
    return;
}

int qssamp(samp)
Sample *samp;
{
    int nc, len;

    nc = samp->nc;
    printf("Begin sort of %d cases\n", nc);
    if (nc < 1) {
        printf("From qssamp: sample unattached.\n");
        return (-1);
    }
    len = samp->reclen;

    qssamp1(samp->recs, nc, len);
    printf("Finished sort\n");
    return (0);
}

/*	--------------------  id2ind  ------------------------------  */
/*	Given a item ident, returns index in sample, or -1 if not found  */
int id2ind(id)
int id;
{
    int iu, il, ic, cid, len;
    char *recs;

    if ((!samp) || (samp->nc == 0)) {
        printf("No defined sample\n");
        return (-1);
    }
    recs = samp->recs + 1;
    len = samp->reclen;
    iu = samp->nc;
    il = 0;
chop:
    ic = (iu + il) >> 1;
    cmcpy(&cid, recs + ic * len, sizeof(int));
    if (ic == il)
        goto chopped;
    if (cid > id) {
        iu = ic;
        goto chop;
    }
    if (cid < id) {
        il = ic;
        goto chop;
    }
chopped:
    return ((cid == id) ? ic : -1);
}

/*	------------------------  thinglist  -------------------   */
/*	Records best class and score for all things in a sample.
 */
int thinglist(tlstname)
char *tlstname;
{
    FILE *tlst;
    int nn, dadser, i, bc, tid, bl;
    double bw, bs;
    Class *clp;

    /*	Check we have an attched sample and model  */
    if (!ctx.popln)
        return (-1);
    if (!ctx.sample)
        return (-2);
    setpop();
    if (!samp->nc)
        return (-3);

    /*	Open a file  */
    tlst = fopen(tlstname, "w");
    if (!tlst)
        return (-4);
    /*	Output a tree list in a primitive form  */
    clp = rootcl;

treeloop:
    if (clp->type == Sub)
        goto nextcl1;
    fprintf(tlst, "%8d", clp->serial >> 2);
    if (clp->idad >= 0)
        dadser = population->classes[clp->idad]->serial;
    else
        dadser = -4;
    fprintf(tlst, "%8d\n", dadser >> 2);
nextcl1:
    nextclass(&clp);
    if (clp)
        goto treeloop;
    fprintf(tlst, "0 0\n");

    findall(Dad + Leaf);

    for (nn = 0; nn < samp->nc; nn++) {
        docase(nn, Leaf + Dad, 0);
        bl = bc = -1;
        bw = 0.0;
        bs = samp->nc + 1;
        for (i = 0; i < numson; i++) {
            clp = sons[i];
            if ((clp->casewt > 0.5) && (clp->cnt < bs)) {
                bc = i;
                bs = clp->cnt;
            }
            if ((clp->type == Leaf) && (clp->casewt > bw)) {
                bl = i;
                bw = clp->casewt;
            }
        }

        cmcpy(&tid, record + 1, sizeof(int));
        fprintf(tlst, "%8d %6d %6d  %6.3f\n", tid, sons[bc]->serial >> 2,
                sons[bl]->serial >> 2, ScoreRscale * sons[bl]->vv[nn]);
    }

    fclose(tlst);
    return (0);
}
