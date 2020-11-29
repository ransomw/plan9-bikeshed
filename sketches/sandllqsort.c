#include <u.h>
#include <libc.h>

typedef struct ll *Ll;
struct ll {
	uint num;
	struct ll *next;
};

static int nums_00[] = {13, 3, 1, 4, 2, 11};

void
terminate(char *s)
{
	exits(s);
}

Ll
nums_to_ll(int nums[], int nnums)
{
	int i;
	Ll curr, next;

	curr = nil;
	for(i = 0; i < nnums; i++) {
		next = (Ll) malloc(sizeof(*next));
		next->num = nums[i];
		next->next = curr;
		curr = next;
	}
	return curr;
}

void
printll(Ll ll)
{
	for(; ll != nil && ll->next != nil; ll = ll->next)
		print("%d ", ll->num);
	if(ll != nil)
		print("%d", ll->num);
	print("\n");
}

Ll
nth(Ll ll, int i)
{
	Ll lli;
	
	lli = ll;
	for(; i > 0; i--) {
		if(lli == nil)
			terminate("nth index out of bounds");
		lli = lli->next;
	}
	if(lli == nil)
		terminate("nth index out of bounds");
	return lli;
}

Ll
swap(Ll ll, int i, int j)
{
	Ll lli, llj;
	Ll lliprev, lljprev;
	Ll llinext, lljnext;

	if(i > j)
		terminate("swap indices in wrong order");
	if(i == j)
		return ll;

	if(i == 0)
		lliprev = nil;
	else
		lliprev = nth(ll, i-1);
	lljprev = nth(ll, j-1);
	lli = nth(ll, i);
	llj = nth(ll, j);
	llinext = lli->next;
	lljnext = llj->next;

	/* blue-gold (cycle free at all times) */
	if(lliprev != nil)
		lliprev->next = llj;
	else
		ll = llj;
	lli->next = lljnext;
	if(i + 1 == j) {
		llj->next = lli;
		return ll;
	}
	lljprev->next = lli;
	llj->next = llinext;
	return ll;
}

Ll
mqsort(Ll ll, int left, int right,
	int (*comp)(void *, void*))
{
	int i, last;

	if(left >= right)
		return ll;
	ll = swap(ll, left, (left + right)/2);
	last = left;
	for(i = left + 1; i <= right; i++)
		if((*comp)(nth(ll, i), nth(ll, left)) < 0)
			ll = swap(ll, ++last, i);
	ll = swap(ll, left, last);
	/* now everything left of last is < left
	 * and everything on the other side is >= left
	 */
	ll = mqsort(ll, left, last - 1, comp);
	ll = mqsort(ll, last + 1, right, comp);
	return ll;
}

int
compnums(Ll lla, Ll llb)
{
	return lla->num - llb->num;
}

void
main(void)
{
	Ll ll = nums_to_ll(nums_00, nelem(nums_00));

	ll = mqsort(ll, 0, nelem(nums_00) - 1,
				(int (*)(void *, void *)) compnums);

	printll(ll);
}

void
testswap(void)
{
	Ll ll = nums_to_ll(nums_00, nelem(nums_00));

	printll(ll);

	print("\n");
	ll = swap(ll, 0, 1);
	print("swap(ll, 0, 1)\n");
	printll(ll);
	ll = swap(ll, 1, 2);
	print("swap(ll, 1, 2)\n");
	printll(ll);
	ll = swap(ll, nelem(nums_00) - 2, nelem(nums_00) - 1);
	print("swap(ll, n - 2, n - 1)\n");
	printll(ll);
	ll = swap(ll, nelem(nums_00) - 3, nelem(nums_00) - 2);
	print("swap(ll, n - 3, n - 2)\n");
	printll(ll);
	ll = swap(ll, 0, nelem(nums_00) - 1);
	print("swap(ll, 0, n - 1)\n");
	printll(ll);
	ll = swap(ll, 1, nelem(nums_00) - 1);
	print("swap(ll, 1, n - 1)\n");
	printll(ll);
	ll = swap(ll, 0, nelem(nums_00) - 2);
	print("swap(ll, 0, n - 2)\n");
	printll(ll);
	ll = swap(ll, 1, nelem(nums_00) - 2);
	print("swap(ll, 1, n - 2)\n");
	printll(ll);

}