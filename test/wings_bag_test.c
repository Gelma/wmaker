/* wings_bag_test.c - index bookkeeping of WMBag.
 *
 * A bag is ordered and addressable by index, so removing an element has to
 * renumber everything after it.  WMEraseFromBag() used to delete the node
 * without that renumbering (Coverity CID 1686055 pointed here while
 * reporting a use-after-free that turned out to be a false positive), which
 * left the indices sparse and made the next lookup by index return NULL or
 * the wrong element.
 */

#include <stdio.h>

#include <WINGs/WINGs.h>

#define COUNT 10

int main(void)
{
	WMBag *bag;
	int i, failed = 0;

	bag = WMCreateTreeBag();
	for (i = 0; i < COUNT; i++)
		WMPutInBag(bag, (void *)(long)(i + 1));

	/* remove the fourth element; what followed it must shift down */
	if (!WMEraseFromBag(bag, 3)) {
		fprintf(stderr, "WMEraseFromBag(3) failed\n");
		return 1;
	}
	if (WMGetBagItemCount(bag) != COUNT - 1) {
		fprintf(stderr, "count is %d, expected %d\n",
		        WMGetBagItemCount(bag), COUNT - 1);
		return 1;
	}

	for (i = 0; i < COUNT - 1; i++) {
		long want = (i < 3) ? i + 1 : i + 2;
		long got = (long)WMGetFromBag(bag, i);

		if (got != want) {
			fprintf(stderr, "after erase, bag[%d] = %ld, expected %ld\n",
			        i, got, want);
			failed = 1;
		}
	}
	if (WMGetFromBag(bag, COUNT - 1) != NULL) {
		fprintf(stderr, "bag[%d] is not empty\n", COUNT - 1);
		failed = 1;
	}
	WMFreeBag(bag);

	if (failed)
		return 1;

	printf("WMBag: erase renumbers the following elements\n");
	return 0;
}
