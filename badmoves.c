#define BADMOVES 1
#define NOTGLOB 1
#include "glob.c"

static char *movestr[] = { " ", "Insert", "Delete", "Move"};

/*	---------------  clearbadm  --------------------------------  */
/*	To clear the badmoves table  */
void clearbadm ()
{
	Sw i;
	for (i = 0; i < BadSize; i++) badkey [i] = 0;
	return;
}

/*	-----------------  testbadm  -----------------------------  */
/*	To test a move for known recent fail  */
/*	code 1 means insert ,code 2 means delete  */
/*	code 3 means move */
Sw testbadm (code, w1, w2)
	Sw code, w1, w2;
{
	Sw hi, key, bad, s1, s2;

	setpop ();
	s1 = w1;  s2 = w2;
	if ((code == 1) || (code == 3))  {
		if (w1 > w2)  { s1 = w2;  s2 = w1; }
		}
	else s1 = 0;
	key = hi = (((code << 13) + s1) << 13) + s2;
	if (hi < 0) hi = -1 - hi;
	hi = hi % BadSize;
	bad = 0;
	if (badkey[hi] != key) goto done;
	
	flp();  bad = 1;
printf ("Badmove rejects %s", movestr [code]);
if (w1) printf("%6d", w1 >> 2);
if (w2) printf("%6d", w2 >> 2);
printf ("\n");
done:	return (bad);
}


/*	------------------  setbadm  -----------------------------   */
/*	To log a bad move  */
void setbadm (code, s1, s2)
	Sw code, s1, s2;
{
	Sw hi, key;

	if ((code == 1) || (code == 3))  {
		if (s1 > s2)  { hi = s2;  s2 = s1;  s1 = hi; }
		}
	else s1 = 0;
	key = hi = (((code << 13) + s1) << 13) + s2;
	setpop ();
	if (hi < 0) hi = -1 - hi;
	hi = hi % BadSize;
	badkey[hi] = key;
	return;
}
