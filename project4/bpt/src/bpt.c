/**
 *    @class Database System
 *    @file  bpt.c
 *    @brief join 
 *    @author Kibeom Kwon (kgbum2222@gmail.com)
 *    @since 2017-11-23
 */       

#include "bpt.h"


// OPEN AND INIT BUF

int cut (int length) {
	if (length % 2 == 0)
		return length / 2;
	else
		return (length / 2) + 1;
}

// Initialize headerpage.
Buf * init_headerpage (int table_id) {
	int i;

	for (i = 0; i < num_buf; i++) 
		if (buf[i].page_offset == PAGE_NONE) 
			break;

	Buf * hb;
	hb = &buf[i];
	hb->table_id = table_id;
	hb->page_offset = HEADERPAGE_OFFSET;
	hb->pin_count = 1;
	hb->is_dirty = 1;

	// Update LRU
	hb->lru->next = LRU_list->head->next;
	LRU_list->head->next->prev = hb->lru;
	hb->lru->prev = LRU_list->head;
	LRU_list->head->next = hb->lru;
	hb->in_LRU = true;
	LRU_list->num_lru++;

	return hb;
}

// Release pin_count of page.
void release_pincount(Buf * b) {
	b->pin_count--;
}

// Mark dirty bit of page.
void mark_dirty(Buf * b) {
	b->is_dirty = true;
}

// Read page from file
void read_page(int table_id, Page * page, int64_t size, int64_t offset) {
	pread(table_id + 2, page, size, offset);
}

// Write page to file
void write_page(int table_id, Page * page, int64_t size, int64_t offset) {
	pwrite(table_id + 2, page, size, offset);
}

/* Initialize buf array before open database file or make database file.
 * Using buf array make reusing page structure.
 */
/*
void init_buf(int table_id) {
	int i, j;
	buf[table_id] = (Buf *)malloc(sizeof(Buf)*BUF_SIZE);

	for (i = 0; i < BUF_SIZE; i++) {
		buf[table_id][i].page = (Page *)malloc(sizeof(Page));
		buf[table_id][i].page_offset = -1;
		buf[table_id][i].table_id = table_id;
		buf[table_id][i].pin_count = 0;
		buf[table_id][i].is_dirty = false;
		buf[table_id][i].in_LRU = false;
		buf[table_id][i].lru = (LRU *)malloc(sizeof(LRU));
	}
}
*/

/* Allocate the buffer pool (array) with the given number of entries.
 * Initialize other fields (state info, LRU info..) with your own design.
 * If success, return 0. Otherwise, return non-zero value.
 */
int init_db(int num) {
	int i;
	num_buf = num;
	buf = (Buf *) malloc(sizeof(Buf) * num_buf);
	for (i = 0; i < num_buf; i++) {
		buf[i].page = (Page *) malloc(sizeof(Page));
		buf[i].table_id = 0;
		buf[i].page_offset = PAGE_NONE;
		buf[i].is_dirty = false;
		buf[i].pin_count = 0;
		buf[i].lru = (LRU *) malloc(sizeof(LRU));
	}
	LRU_list = (LRU_LIST *)malloc(sizeof(LRU_LIST));
	LRU * dummy_head = (LRU *)malloc(sizeof(LRU));
	LRU * dummy_tail = (LRU *)malloc(sizeof(LRU));
	dummy_head->next = dummy_tail;
	dummy_tail->prev = dummy_head;
	LRU_list->head = dummy_head;
	LRU_list->tail = dummy_tail;
	LRU_list->num_lru = 0;
	return 0;
}

/* Make victim page and remove.
 */
void make_victim() {
	Buf * vb;
	LRU * d_lru;

	while (1) {
		d_lru = LRU_list->tail->prev;

		// Remove in LRU_list.
		d_lru->prev->next = LRU_list->tail;
		LRU_list->tail->prev = d_lru->prev; 

		vb = d_lru->buf;

		if (vb->pin_count != 0) {
			// If victim page is using, current page moves to head of LRU_list
			d_lru->next = LRU_list->head->next;
			LRU_list->head->next->prev = d_lru;
			d_lru->prev = LRU_list->head;
			LRU_list->head->next = d_lru;
			continue;
		}
		break;
	}
		
	if (vb->is_dirty) {
		write_page(vb->table_id, vb->page, PAGE_SIZE, vb->page_offset);
	}

	vb->is_dirty = false;
	vb->in_LRU = false;
	vb->page_offset = PAGE_NONE;

	LRU_list->num_lru--;
}

/* Upate LRU.
 * If success return 0. Otherwise, return non_zero value.
 */
int update_LRU(Buf * b) {
	if (LRU_list->num_lru >= num_buf && b->in_LRU == false) {
		make_victim();
	}

	b->lru->buf = b;
	
	// If page already exists in buffer.
	if (b->in_LRU == true) {
		b->lru->prev->next = b->lru->next;
		b->lru->next->prev = b->lru->prev; 
		LRU_list->num_lru--;
	}

	b->lru->next = LRU_list->head->next;
	LRU_list->head->next->prev = b->lru;
	b->lru->prev = LRU_list->head;
	LRU_list->head->next = b->lru;
	b->in_LRU = true;

	LRU_list->num_lru++;
	if (LRU_list->num_lru > num_buf) {
		printf("update_LRU() error!!!!\n");
		return -1;
	}

	b->pin_count++;

	return 0;
}
	
/* Find Buf structure of its offset.
 * If exist, return buf pointer.
 * If not, return NULL
 */
Buf * find_buf(int table_id, int64_t offset) {
	int i;

	// If not exist.
	// -1 is not read.
	/*
	if (buf[table_id][buf_idx].page_offset == -1 || buf[table_id][buf_idx].in_LRU != true)
		return NULL;
	*/

	// Find page in buffer pool.
	for (i = 0; i < num_buf; i++) {
		if (buf[i].table_id == table_id && buf[i].page_offset == offset) {
			update_LRU(&buf[i]);
			return &buf[i];
		}
	}

	// If not exist
	return NULL;
}

/* Make Buf structure
 */
Buf * make_buf(int table_id, int64_t offset) {
	free_page * fp;
	header_page * hp;
	Buf * hb;
	
	int i;
	int64_t free_offset;

	free_offset = offset;
	
	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;

	// If buffer is full
	if (LRU_list->num_lru == num_buf) {
		make_victim();
		for (i = 0; i < num_buf; i++) {
			if (buf[i].page_offset == PAGE_NONE) {
				break;
			}
		}
		if (i == num_buf) {
			printf("All page is used!!!!\n");
			return;
		}
	} else {
		for (i = 0; i < num_buf; i++) {
			if (buf[i].page_offset == PAGE_NONE) {
				break;
			}
		}
	}

	// If free page assigned
	if (hp->free_page == offset) {
		fp = (free_page *)malloc(sizeof(free_page));
		read_page(table_id, (Page *)fp, PAGE_SIZE, offset);

		if (fp->next_page == 0) {
		// If next freepage doesn't exist
			while (1) {
				free_offset += PAGE_SIZE;
				read_page(table_id, (Page *)fp, PAGE_SIZE, free_offset);
				if (fp->next_page == 0 && hp->root_page != free_offset) {
					hp->free_page = free_offset;
					break;
				}
			}
		} else {
		// If next freepage exists
			hp->free_page = fp->next_page;
		}

		hp->num_pages++;
		mark_dirty(hb);
		release_pincount(hb);
		free(fp);

	} else {
	// If read page first
		read_page(table_id, buf[i].page, PAGE_SIZE, offset);
	}
	
	buf[i].page_offset = offset;
	buf[i].table_id = table_id;
	buf[i].is_dirty = false;
	if (update_LRU(&buf[i]) != 0) {
		printf("make_buf(update_LRU)) error!!!\n");
	}
	
	return &buf[i];
}

/* To get Buf structure of its offset
 * if page already exists, get Buf structure.
 * If not, assign Buf and read page of its offset.
 */
Buf * get_buf(int table_id, int64_t offset) {
	Buf * b;
	
	if ((b = find_buf(table_id, offset)) != NULL)
		return b;

	// If page is first read, read page
	return make_buf(table_id, offset);
}

/* Open existing data file using ‘pathname’ or create one if not existed.
 * If success, return table_id. Otherwise, return -1.
 */
int open_table(char* pathname) {
	int fd;
	int table_id;
	header_page * hp;
	Buf * hb;
	 
	hp = (header_page *)malloc(sizeof(header_page));
	// If file exists
	if (access(pathname, 0) == 0) {
		if ((fd = open(pathname, O_RDWR | O_SYNC, 0644)) == -1) {
			// Fail to access
			return -1;
		} else {
			table_id = fd - 2;
			// Success to access, read header page
			hb = init_headerpage(table_id);
			read_page(table_id, hb->page, PAGE_SIZE, HEADERPAGE_OFFSET);
			hp = (header_page *)hb->page;
			printf("root_offset: %ld \n", hp->root_page);
		//	update_LRU(hb);
			release_pincount(hb);
			return table_id;
		}
	} else {
		// If file doesn't exist, make file and initialize header page and write into file.
		if ((fd = open(pathname, O_CREAT | O_RDWR | O_SYNC, 0644)) == -1) {
			// Fail to make file.
			return -1;
		} else {
			table_id = fd - 2;
			//init_buf(table_id);
			// Success to make file, initialize header page and write into file.
			hb = init_headerpage(table_id);
			hp = (header_page *)hb->page;
			hp->free_page = PAGE_SIZE;
			hp->root_page = 0;
			hp->num_pages = 1;	// header page

			//	update_LRU(hb);

			// Make root page. 
			// First root page is leaf page.
			Buf * b = get_buf(table_id, hp->free_page);
			leaf_page * root = (leaf_page *) b->page;
			root->parent_page = 0;
			root->is_leaf = 1;
			root->num_keys = 0;
			root->right_sibling = 0;

			hp->root_page = b->page_offset;

			hp->num_pages++;

			// write page into disk
			mark_dirty(b);
			mark_dirty(hb);

			release_pincount(b);
			release_pincount(hb);
			return table_id;
		}
	}
}

int close_table(int table_id) {
	LRU * cur;
	cur = LRU_list->head->next;
	while (cur != LRU_list->tail) {
		if (cur->buf->table_id == table_id) {
			Buf * vb;

			// Remove in LRU_list.
			cur->prev->next = cur->next;
			cur->next->prev = cur->prev; 

			vb = cur->buf;

			if (vb->is_dirty) {
				write_page(vb->table_id, vb->page, PAGE_SIZE, vb->page_offset);
			}

			vb->is_dirty = false;
			vb->in_LRU = false;
			vb->page_offset = PAGE_NONE;

			LRU_list->num_lru--;

			cur = cur->next;

		}
	}
	return 0;
}

int shutdouwn(int table_id) {
	LRU * cur;
	cur = LRU_list->head->next;
	while (cur != LRU_list->tail) {
		Buf * vb;

		// Remove in LRU_list.
		cur->prev->next = cur->next;
		cur->next->prev = cur->prev; 

		vb = cur->buf;

		if (vb->is_dirty) {
			write_page(vb->table_id, vb->page, PAGE_SIZE, vb->page_offset);
		}

		vb->is_dirty = false;
		vb->in_LRU = false;
		vb->page_offset = PAGE_NONE;

		LRU_list->num_lru--;

		cur = cur->next;
	}
	if (LRU_list->num_lru == 0) {
		return 0;
	}
	return -1;
}


// FIND < KEY >

/* Find Buf pointer of leaf page which has key.
 */
Buf * find_leaf(int table_id, int64_t key) {
	int i;
	Buf * hb;
	header_page * hp;
	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;

	Buf * b = get_buf(table_id, hp->root_page);
	internal_page * c = (internal_page *) b->page;

	while (!c->is_leaf) {
		release_pincount(b);
		i = 0;
		while (i < c->num_keys) {
			if (key >= c->records[i].key)
				i++;
			else
				break;
		}
		if (i == 0) {
			b = get_buf(table_id, c->one_more_page);
			c = (internal_page *) b->page;
		} else {
			b = get_buf(table_id, c->records[i - 1].page_offset);
			c = (internal_page *) b->page;
		}
	}

	release_pincount(hb);
	return b;
}

/* Finds and returns the record to which
 * a key refers.
 */
char * find(int table_id, int64_t key) {
	int i;
	Buf * b = find_leaf(table_id, key);
	leaf_page * leaf = (leaf_page *) b->page;
	release_pincount(b);
	if (b->page_offset == 0) return NULL;
	for (i = 0; i < leaf->num_keys; i++) {
		if (leaf->records[i].key == key) break;
	}
	if (i == leaf->num_keys)
		return NULL;
	else {
		return (char *) leaf->records[i].value;
	}

}

// INSERT <KEY> <VALUE>

/* Inserts a new value to a record and its corresponding
 * key into a leaf.
 * Returns 0.
 */
int insert_into_leaf(Buf * b, int64_t key, char * value) {

	printf("insert_into_leaf : %ld \n", key);

	int i, insertion_point;
	leaf_page * leaf = (leaf_page *) b->page;
	
	insertion_point = 0;
	while (insertion_point < leaf->num_keys && leaf->records[insertion_point].key < key)
		insertion_point++;

	for (i = leaf->num_keys; i > insertion_point; i--) {
		leaf->records[i].key = leaf->records[i - 1].key;
		memcpy(leaf->records[i].value, leaf->records[i - 1].value, VALUE_SIZE);
	}

	leaf->records[insertion_point].key = key;
	memcpy(leaf->records[insertion_point].value, value, VALUE_SIZE);
	leaf->num_keys++;

	// Write to disk.
	mark_dirty(b);
	release_pincount(b);

	return 0;
}
/* Inserts a new key and page offset
 * to a new record into a leaf so as to exceed
 * the tree's order, causing the leaf to be split
 * in half.
 */
int insert_into_leaf_after_splitting(int table_id, Buf * b, int64_t key, char * value) {

	printf("insert_into_leaf_after_splitting : %ld \n", key);
	
	Buf * new_b, * hb;
	int insertion_index, split, new_key, i, j;
	leaf_page * leaf, * new_leaf;
	header_page * hp;
	int temp_keys[LEAF_ORDER];
	char temp_values[LEAF_ORDER][VALUE_SIZE];

	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;
	release_pincount(hb);

	new_b = get_buf(table_id, hp->free_page);
	new_leaf = (leaf_page *)new_b->page;

	insertion_index = 0;
	leaf = (leaf_page *) b->page;

	while (insertion_index < LEAF_ORDER - 1 && leaf->records[insertion_index].key < key)
		insertion_index++;

	for (i = 0, j = 0; i < leaf->num_keys; i++, j++) {
		if (j == insertion_index) j++;
		temp_keys[j] = leaf->records[i].key;
		memcpy(temp_values[j], leaf->records[i].value, VALUE_SIZE);
	}

	temp_keys[insertion_index] = key;
	memcpy(temp_values[insertion_index], value, VALUE_SIZE);

	leaf->num_keys = 0;
	new_leaf->num_keys = 0;

	split = cut(LEAF_ORDER);

	for (i = 0; i < split; i++) {
		memcpy(leaf->records[i].value, temp_values[i], VALUE_SIZE);
		leaf->records[i].key = temp_keys[i];
		leaf->num_keys++;
	}

	for (i = split, j = 0; i < LEAF_ORDER; i++, j++) {
		memcpy(new_leaf->records[j].value, temp_values[i], VALUE_SIZE);
		new_leaf->records[j].key = temp_keys[i];
		new_leaf->num_keys++;
	}

	leaf->right_sibling = new_b->page_offset;
	new_leaf->parent_page = leaf->parent_page;
	new_leaf->is_leaf = 1;
	new_key = new_leaf->records[0].key;

	// Write to disk
	mark_dirty(b);
	mark_dirty(new_b);

	return insert_into_parent(table_id, b, new_key, new_b);
}

/* Inserts a new key and page_offset into internal page
 * causing the page's size to exceed
 * the INTERNAL_OREDER, and causing the page to split into two.
 */
int insert_into_internal_after_splitting(int table_id, Buf * b, int left_index, 
		int64_t key, Buf * right_b) {
	
	printf("insert_into_internal_after_splitting : %ld \n", key);

	int i, j, split, k_prime;
	Buf * new_b, * child_b, * hb;
	internal_page * new_page, * child;
	internal_page * old_page, right;
	header_page * hp;
	int64_t temp_keys[INTERNAL_ORDER];
	int64_t temp_pageoffset[INTERNAL_ORDER];

	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;
	release_pincount(hb);

	old_page = (internal_page *)b->page;

	for (i = 0, j = 0; i < old_page->num_keys; i++, j++) {
		if (j == left_index) j++;
		temp_pageoffset[j] = old_page->records[i].page_offset;
		temp_keys[j] = old_page->records[i].key;
	}
	temp_pageoffset[left_index] = right_b->page_offset;
	temp_keys[left_index] = key;

	split = cut (INTERNAL_ORDER);

	new_b = get_buf(table_id, hp->free_page);
	new_page = (internal_page *)new_b->page;
	new_page->is_leaf = false;
	new_page->num_keys = 0;
	old_page->num_keys = 0;

	for (i = 0; i < split; i++) {
		old_page->records[i].page_offset = temp_pageoffset[i];
		old_page->records[i].key = temp_keys[i];
		old_page->num_keys++;
	}

	new_page->one_more_page = temp_pageoffset[i];
	k_prime = temp_keys[i];

	for (++i, j = 0; i < INTERNAL_ORDER; i++, j++) {
		new_page->records[j].page_offset = temp_pageoffset[i];
		new_page->records[j].key = temp_keys[i];
		new_page->num_keys++;
	}

	new_page->parent_page = old_page->parent_page;

	child_b = get_buf(table_id, new_page->one_more_page);
	child = (internal_page *)child_b->page;
	child->parent_page = new_b->page_offset;
	mark_dirty(child_b);
	release_pincount(child_b);

	for (i = 0; i < new_page->num_keys; i++) {
		child_b = get_buf(table_id, new_page->records[i].page_offset);
		child = (internal_page *)child_b->page;
		child->parent_page = new_b->page_offset;
		mark_dirty(child_b);
		release_pincount(child_b);
	}

	mark_dirty(b);
	mark_dirty(new_b);

	release_pincount(right_b);

	return insert_into_parent(table_id, b, k_prime, new_b); 
}

/* Helper funciton used in insert_into_parent
 * to find the index of the parent's key to 
 * the page to the left of the key to be inserted.
 */
int get_left_index(Buf * b, Buf * left_b) {
	
	internal_page * parent;
	int left_index = 0;
	parent = (internal_page *) b->page;

	if (parent->one_more_page == left_b->page_offset)
		return 0;

	while (left_index <= parent->num_keys &&
			parent->records[left_index].page_offset != left_b->page_offset)
		left_index++;
	return left_index + 1;
}

/* Inserts a new key and page offset to a internal page
 * into which these can fit
 * without violating the B+ tree properties.
 */
int insert_into_internal(Buf * b, int left_index, int64_t key, Buf * right_b) {

	printf("insert_into_internal : %ld \n", key);
	int i;
	internal_page * parent;
	parent = (internal_page *)b->page;

	for (i = parent->num_keys; i > left_index; i--) {
		parent->records[i].page_offset = parent->records[i - 1].page_offset;
		parent->records[i].key = parent->records[i - 1].key;
	}
	parent->records[left_index].page_offset = right_b->page_offset;
	parent->records[left_index].key = key;
	parent->num_keys++;

	// Write to disk
	mark_dirty(b);

	release_pincount(b);
	release_pincount(right_b);
	return 0;
}

/* Inserts a new key into the B+ tree.
 */
int insert_into_parent(int table_id, Buf * left_b, int64_t key, Buf * right_b) {
	int left_index;
	internal_page * parent;
	internal_page * left;
	header_page * hp;
	Buf * b, * hb;

	left = (internal_page *) left_b->page;
	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;
	release_pincount(hb);

	/* Case : new root. */
	if (left_b->page_offset == hp->root_page) 
		return insert_into_new_root(table_id, left_b, key, right_b);

	/* Case : leaf or internal page.
	 */

	/* Find the parent's pointer to the left page.
	 */

	b = get_buf(table_id, left->parent_page);
	left_index = get_left_index(b, left_b);
	release_pincount(left_b);

	/* Simple case : the new key fits into the page.
	 */

	parent = (internal_page *)b->page;
	if (parent->num_keys < INTERNAL_ORDER - 1)
		return insert_into_internal(b, left_index, key, right_b);

	/* Harder case : split a page in order
	 * to preserve the B+ tree properties.
	 */

	return insert_into_internal_after_splitting(table_id, b, left_index, key, right_b);
}

/* Creates a new root page for two subtrees
 * and inserts the appropriate key into
 * the new root page.
 */
int insert_into_new_root(int table_id, Buf * left_b, int64_t key, Buf * right_b) {

	printf("insert_into_new_root : %ld \n", key);
	Buf * root_b, * hb;
	internal_page * root, * left, * right;
	header_page * hp;
	
	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;

	root_b = get_buf(table_id, hp->free_page);
	root = (internal_page *)root_b->page;

	left = (internal_page *)left_b->page;
	right = (internal_page *)right_b->page;

	root->records[0].key = key;
	root->one_more_page = left_b->page_offset;
	root->records[0].page_offset = right_b->page_offset;
	root->num_keys = 1;
	root->parent_page = 0;
	root->is_leaf = false;
	left->parent_page = root_b->page_offset;
	right->parent_page = root_b->page_offset;
	hp->root_page = root_b->page_offset;

	// Write to disk
	mark_dirty(root_b);
	mark_dirty(left_b);
	mark_dirty(right_b);
	mark_dirty(hb);

	release_pincount(root_b);
	release_pincount(left_b);
	release_pincount(right_b);
	release_pincount(hb);

	return 0;
}

/* Master insertion function.
 * Inserts a key and an associated value into
 * the B+ tree, causing the tree to be adjusted
 * however necessary to maintain the B+ tree
 * properties.
 */
int insert(int table_id, int64_t key, char * value) {

	Buf * b;
	leaf_page * leaf;
	
	// If key is duplicate 
	if (find(table_id, key) != NULL) {
		printf("error : INSERT DUPLICATE KEY <%ld> !!!\n", key);
		return -1; 
	}

	/* Case : the tree already exists.
	 */

	b = find_leaf(table_id, key);
	leaf = (leaf_page *) b->page;

	/* Case : leaf has room for key.
	 */

	if (leaf->num_keys < LEAF_ORDER - 1) {
		return insert_into_leaf(b, key, value);
	}

	/* Case : leaf must be split.
	 */

	return insert_into_leaf_after_splitting(table_id, b, key, value);
}

// DELETE <KEY>

/* Utility function for deletion. Retrieves
 * the index of a page's nearest neighbor (silbing)
 * to the left if one exists. If no (the page is
 * the leftmost child), returns -1 to signify
 * this special case.
 */
int get_neighbor_index(int table_id, Buf * b) {
	int i;
	internal_page * c, * parent;
	Buf * pb;
	c = (internal_page *)b->page;
	pb = get_buf(table_id, c->parent_page);
	parent = (internal_page *)pb->page;
	release_pincount(pb);

	if (parent->one_more_page == b->page_offset)
		return -2;

	for (i = 0; i <= parent->num_keys; i++)
		if (parent->records[i].page_offset == b->page_offset)
			return i - 1;
}

Buf * remove_entry_from_page(Buf * b, int64_t key) {

	int i, num_pointers;
	internal_page * c;
	leaf_page * leaf;
	c = (internal_page *)b->page;

	if (c->is_leaf) {
		// If page is leaf page.
		leaf = (leaf_page *)b->page;

		// Remove the key and shift other keys accordingly.
		i = 0;
		while (leaf->records[i].key != key && i < leaf->num_keys) {
			i++;
		}
		for (++i; i < leaf->num_keys; i++) {
			leaf->records[i - 1].key = leaf->records[i].key;
			memcpy(leaf->records[i - 1].value, leaf->records[i].value, VALUE_SIZE);
		}
		leaf->num_keys--;
		mark_dirty(b);
	} else {
		// If page is internal page.
		i = 0;
		while (c->records[i].key != key)
			i++;
		for (++i; i < c->num_keys; i++) {
			c->records[i - 1].key = c->records[i].key;
			c->records[i - 1].page_offset = c->records[i].page_offset;
		}
		c->num_keys--;
		mark_dirty(b);
	}
	return b;
}

int adjust_root(int table_id, Buf * b) {

	Buf * nb, *hb;
	internal_page * nroot;
	header_page * hp;
	internal_page * root = (internal_page *)b->page;
	
	/* Case : nonempty root.
	 * Key and value have already been deleted,
	 * so nothing to be done.
	 */

	if (root->num_keys > 0) {
		release_pincount(b);
		return 0;
	}

	/* Case : empty root.
	 */

	// If it has a child, promote
	// the first (only) child
	// as the new root.
	
	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;
	
	if (!root->is_leaf) {
		hp->root_page = root->one_more_page;
		nb = get_buf(table_id, root->one_more_page);
		nroot = (internal_page *)nb->page;
		nroot->parent_page = 0;
		b->page_offset = -1;

		mark_dirty(hb);
		mark_dirty(nb);
		mark_dirty(b);

		release_pincount(hb);
		release_pincount(b);
		release_pincount(nb);

	}
	return 0;
}

/* Coalesces a page that has become
 * too small after deletion
 * with a neighboring page that
 * can accept the additional entries
 * without exceeding the maximum.
 */
int coalesce_pages (int table_id, Buf * b, Buf * nb, int neighbor_index, int64_t k_prime) {

	int i, j, neighbor_insertion_index, n_end;
	Buf * temp, * child, * parent_b, * hb;
	internal_page * ci, *ni, * cp;
	leaf_page * cl, *nl;
	header_page * hp;

	/* Swap neighbor with page if page is on the
	 * extreme left and neighbor is to its right.
	 */

	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;

	if (neighbor_index == -2) {
		temp = b;
		b = nb;
		nb = temp;
	}
	
	/* Starting point in the neighbor for copyunh
	 * keys and values from b.
	 * Recall that b and neighbor have swapped places
	 * in the special case of b being leftmost child.
	 */

	ni = (internal_page *) nb->page;
	parent_b = get_buf(table_id, ni->parent_page);
	
	neighbor_insertion_index = ni->num_keys;

	/* Case : nonleaf page.
	 * Append k_prime and the following page offset.
	 * Append all page offset and keys from the neighbor.
	 */

	if (!ni->is_leaf) {
		ci = (internal_page *)b->page;
		ni->records[neighbor_insertion_index].key = k_prime;
		ni->records[neighbor_insertion_index].page_offset = ci->one_more_page;
		ni->num_keys++;

		n_end = ci->num_keys;

		for (i = neighbor_insertion_index + 1, j = 0; j < n_end; i++, j++) {
			ni->records[i].key = ci->records[j].key;
			ni->records[i].page_offset = ci->records[j].page_offset;
			ni->num_keys++;
			ci->num_keys--;
		}

		/* All children must now point up to the same parent.
		 */
		child = get_buf(table_id, ni->one_more_page);
		cp = (internal_page *)child->page;
		cp->parent_page = nb->page_offset;
		mark_dirty(child);
		release_pincount(child);
		for (i = 0; i < ni->num_keys; i++ ) {
			child = get_buf(table_id, ni->records[i].page_offset);
			cp = (internal_page *)child->page;
			cp->parent_page = nb->page_offset;
			mark_dirty(child);
			release_pincount(child);
		}

		ci->parent_page = 0;
		hp->num_pages--;
		b->page_offset = 0;

	}

	/* In a leaf, append the keys and value of 
	 * b to nb.
	 */
	
	else {
		cl = (leaf_page *)b->page;
		nl = (leaf_page *)nb->page;
		for (i = neighbor_insertion_index, j = 0; j < cl->num_keys; i++, j++) {
			nl->records[i].key = cl->records[j].key;
			memcpy(nl->records[i].value, cl->records[j].value, VALUE_SIZE);
			nl->num_keys++;
		}
		nl->right_sibling = cl->right_sibling;

		cl->parent_page = 0;
		hp->num_pages--;
		b->page_offset = 0;
	}
	mark_dirty(hb);
	mark_dirty(b);
	mark_dirty(nb);
	
	release_pincount(hb);
	release_pincount(b);
	release_pincount(nb);

	return delete_entry(table_id, parent_b, k_prime);
}

/* Redistributes entries between two pages when
 * one has become too small after deletion
 * but its neighbor is too big to append the
 * small page's entries without exceeding the
 * maximum.
 */

int redistribute_pages(int table_id, Buf * b, Buf * nb, int neighbor_index, 
		int k_prime_index, int k_prime) {
	int i;
	internal_page * ci, * ni, * parent;
	leaf_page * cl, * nl;
	Buf * tmp, * pb;

	/* Case : b has a neighbor to the left.
	 * Pull the neighbor's last key-pointer pair over
	 * from the neighbor's right end to b's left end.
	 */

	ci = (internal_page *)b->page;
	pb = get_buf(table_id, ci->parent_page);
	parent = (internal_page *)pb->page;

	if (neighbor_index != -2) {
		if (ci->is_leaf) {
			// If page is leaf
			cl = (leaf_page *)b->page;
			nl = (leaf_page *)nb->page;

			for (i = cl->num_keys; i > 0; i--) {
				cl->records[i].key = cl->records[i - 1].key;
				memcpy(cl->records[i].value, cl->records[i - 1].value, VALUE_SIZE);
			}
			cl->records[0].key = nl->records[nl->num_keys - 1].key;
			memcpy(cl->records[0].value, nl->records[nl->num_keys - 1].value, VALUE_SIZE);
			parent->records[k_prime_index].key = cl->records[0].key;

			nl->records[nl->num_keys - 1].key = -1;
			cl->num_keys--;
			nl->num_keys++;
					
		} else {
			// If page is internal page
			ni = (internal_page *)nb->page;
			for (i = cl->num_keys; i > 0; i--) {
				ci->records[i].key = ci->records[i - 1].key;
				ci->records[i].page_offset = ci->records[i - 1].page_offset;
			}
			ci->records[0].key = k_prime;
			ci->records[0].page_offset = ci->one_more_page;
			ci->one_more_page = ni->records[ni->num_keys - 1].page_offset;
			parent->records[k_prime_index].key = ni->records[ni->num_keys - 1].key;

			ci->num_keys++;
			ni->num_keys--;

		}
	}

	/* Case : b is the leftmost child.
	 * Take a key-value pair frome the neighbor to the right.
	 * Move the neighbor's leftmost key-value pair
	 * to b's rightmost position.
	 */
	else {
		if (ci->is_leaf) {
		// If page is leaf.
		cl = (leaf_page *)b->page;
		nl = (leaf_page *)nb->page;

		cl->records[cl->num_keys].key = nl->records[0].key;
		memcpy(cl->records[cl->num_keys].value, nl->records[0].value, VALUE_SIZE);
		parent->records[k_prime_index].key = nl->records[1].key;

		for (i = 0; i < nl->num_keys - 1; i++) {
			nl->records[i].key = nl->records[i + 1].key;
			memcpy(nl->records[i].value, nl->records[i + 1].value, VALUE_SIZE);
		}
		nl->num_keys--;
		cl->num_keys++;

		} else {
			ni = (internal_page *)nb->page;
			ci->records[ci->num_keys].key = k_prime;
			ci->records[ci->num_keys].page_offset = ni->one_more_page;
			parent->records[k_prime_index].key = ni->records[0].key;
			ni->one_more_page = ni->records[0].page_offset;

			for (i = 0; i < ni->num_keys - 1; i++) {
				ni->records[i].key = ni->records[i + 1].key;
				ni->records[i].page_offset = ni->records[i + 1].page_offset;
			}
			ni->num_keys--;
			ci->num_keys++;

		}
	}
	mark_dirty(b);
	mark_dirty(pb);
	mark_dirty(nb);

	release_pincount(b);
	release_pincount(pb);
	release_pincount(nb);
	return 0;
}



/* Deletes an entry from the B+ tree.
 * Removes the record and its key and value
 * from the leaf page, and then makes all appropriate
 * changes to preserve the B+ tree properties.
 */
int delete_entry(int table_id, Buf * b, int64_t key) {
	int min_keys;
	Buf * nb, * pb, * hb;
	int neighbor_index;
	int64_t k_prime, k_prime_index, nb_offset;
	int capacity;
	internal_page * ipage, * parent, * neighbor;
	header_page * hp;

	// Remove key and value from page.
	
	b = remove_entry_from_page(b, key);

	hb = get_buf(table_id, HEADERPAGE_OFFSET);
	hp = (header_page *)hb->page;
	release_pincount(hb);

	/* Case : deletion from the root.
	 */
	if (b->page_offset == hp->root_page)
		return adjust_root(table_id, b);

	/* Case : deletion from a page below the root.
	 */

	/* Determine minimum allowable size of page,
	 * to be preserved after deletion.
	 */

	ipage = (internal_page *) b->page;

	min_keys = ipage->is_leaf ? cut(LEAF_ORDER - 1) : cut(INTERNAL_ORDER - 1) - 1;

	/* Case : page stays at or above minimum.
	 * (The simple case.)
	 */

	if (ipage->num_keys >= min_keys) {
		release_pincount(b);
		return 0;
	}

	/* Case : page falls below minumum.
	 * Either coalescence or redistribution
	 * is needed.
	 */

	/* Find the appropriate neighbor page with which
	 * to coalesce.
	 * Also find the key (k_prime) in the parent
	 * between the pointer to page b and pointer
	 * to the neighbor.
	 */
	
	pb = get_buf(table_id, ipage->parent_page);
	parent = (internal_page *)pb->page;
	release_pincount(pb);

	neighbor_index = get_neighbor_index(table_id, b);
	k_prime_index = neighbor_index == -2 ? 0 : neighbor_index + 1;
	k_prime = parent->records[k_prime_index].key;
	if (neighbor_index == -2) {
		nb_offset = parent->records[0].page_offset;
	} else if (neighbor_index == -1){
		nb_offset = parent->one_more_page;
	} else {
		nb_offset = parent->records[neighbor_index].page_offset;
	}

	nb = get_buf(table_id, nb_offset);
	neighbor = (internal_page *) nb->page;
	capacity = ipage->is_leaf ? LEAF_ORDER - 1 : INTERNAL_ORDER - 1;

	/* Coalescence. */

	if (neighbor->num_keys + ipage->num_keys <= capacity)
		return coalesce_pages(table_id, b, nb, neighbor_index, k_prime);

	/* Redistribution. */
	else
		return redistribute_pages(table_id, b, nb, neighbor_index, k_prime_index, k_prime);
}


/* Master deletion function.
 */
int delete(int table_id, int64_t key) {

	Buf * b;
	
	if (find(table_id, key) == NULL) {
		printf("key : %ld doesn't exist.\n", key);
		return 0;
	}

	b = find_leaf(table_id, key);
	return delete_entry(table_id, b, key);
}
