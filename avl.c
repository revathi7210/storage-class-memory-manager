/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * avl.c
 */

#include "scm.h"
#include "avl.h"

struct avl {
	struct state {
		uint64_t items;
		uint64_t unique;
		struct node {
			int depth;
			uint64_t count;
			const char *item;
			struct node *left;
			struct node *right;
		} *root;
	} *state; /* SCM */
	struct scm *scm;
};

static int
delta(const struct node *node)
{
	return node ? node->depth : -1;
}

static int
balance(const struct node *node)
{
	return delta(node->left) - delta(node->right);
}

static int
depth(const struct node *a, const struct node *b)
{
	return (delta(a) > delta(b)) ? (delta(a) + 1) : (delta(b) + 1);
}

static struct node *
rotate_right(struct node *node)
{
	struct node *root;

	root = node->left;
	node->left = root->right;
	root->right = node;
	node->depth = depth(node->left, node->right);
	root->depth = depth(root->left, node);
	return root;
}

static struct node *
rotate_left(struct node *node)
{
	struct node *root;

	root = node->right;
	node->right = root->left;
	root->left = node;
	node->depth = depth(node->left, node->right);
	root->depth = depth(root->right, node);
	return root;
}

static struct node *
rotate_left_right(struct node *node)
{
	node->left = rotate_left(node->left);
	return rotate_right(node);
}

static struct node *
rotate_right_left(struct node *node)
{
	node->right = rotate_right(node->right);
	return rotate_left(node);
}

static struct node *
update(struct avl *avl, struct node *root, const char *item)
{
	int d;

	if (!root) {
		if (!(root = scm_malloc(avl->scm, sizeof (struct node)))) {
			TRACE(0);
			return NULL;
		}
		memset(root, 0, sizeof (struct node));
		if (!(root->item = scm_strdup(avl->scm, item))) {
			TRACE(0);
			return NULL;
		}
		++root->count;
		++avl->state->items;
		++avl->state->unique;
		return root;
	}
	if (!(d = strcmp(item, root->item))) {
		++root->count;
		++avl->state->items;
	}
	else if (0 > d) {
		root->left = update(avl, root->left, item);
		if (1 < abs(balance(root))) {
			if (0 > strcmp(item, root->left->item)) {
				root = rotate_right(root);
			}
			else {
				root = rotate_left_right(root);
			}
		}
	}
	else if (0 < d) {
		root->right = update(avl, root->right, item);
		if (1 < abs(balance(root))) {
			if (0 < strcmp(item, root->right->item)) {
				root = rotate_left(root);
			}
			else {
				root = rotate_right_left(root);
			}
		}
	}
	root->depth = depth(root->left, root->right);
	return root;
}

static void
traverse(struct node *node, avl_fnc_t fnc, void *arg)
{
	if (node) {
		traverse(node->left, fnc, arg);
		fnc(arg, node->item, node->count);
		traverse(node->right, fnc, arg);
	}
}

struct avl *
avl_open(const char *pathname, int truncate)
{
	struct avl *avl;

	assert( pathname );

	if (!(avl = malloc(sizeof (struct avl)))) {
		TRACE("out of memory");
		return NULL;
	}
	memset(avl, 0, sizeof (struct avl));
	if (!(avl->scm = scm_open(pathname, truncate))) {
		avl_close(avl);
		TRACE(0);
		return NULL;
	}
	if (scm_utilized(avl->scm)) {
		avl->state = scm_mbase(avl->scm);
	}
	else {
		if (!(avl->state = scm_malloc(avl->scm,
					      sizeof (struct state)))) {
			avl_close(avl);
			TRACE(0);
			return NULL;
		}
		memset(avl->state, 0, sizeof (struct state));
		assert( avl->state == scm_mbase(avl->scm) );
	}
	return avl;
}

void
avl_close(struct avl *avl)
{
	if (avl) {
		scm_close(avl->scm);
		memset(avl, 0, sizeof (struct avl));
	}
	FREE(avl);
}

int
avl_insert(struct avl *avl, const char *item)
{
	struct node *root;

	assert( avl );
	assert( safe_strlen(item) );

	if (!(root = update(avl, avl->state->root, item))) {
		TRACE(0);
		return -1;
	}
	avl->state->root = root;
	return 0;
}

/* Utility function to find the minimum node in a subtree */
static struct node *find_next_min_node(struct node *node) {
    /* Traverse to the leftmost node in the subtree, which is the smallest node */
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

/* Removes the minimum node in a given subtree */
static struct node *remove_min(struct node *node) {
    /* If there's no left child, return the right child to replace this node */
    if (!node->left) {
        return node->right;
    }

    /* Recursively remove the minimum node on the left subtree */
    node->left = remove_min(node->left);

    /* Rebalance if needed */
    if (1 < abs(balance(node))) {
        if (0 < balance(node->right)) {
            node = rotate_right_left(node);
        } else {
            node = rotate_left(node);
        }
    }

    /* Update node depth */
    node->depth = depth(node->left, node->right);
    return node;
}

/* Remove a specific node with a given item from the AVL tree */
static struct node *remove_node(struct avl *avl, struct node *node, const char *item) {
    int cmp_result;

    /* Base case: item not found */
    if (!node) {
        return NULL;
    }

    /* Compare the item with the current node's item */
    cmp_result = strcmp(item, node->item);

    if (cmp_result == 0) {  /* Found the node to be removed */
        struct node *min;

        /* If count > 1, only decrement the count (do not delete the node) */
        if (node->count > 1) {
            --node->count;
            --avl->state->items;
            return node;
        }

        /* Case with only 1 or no children */
        if (!node->left || !node->right) {
            struct node *child = node->left ? node->left : node->right;
            
            /* Free memory for node item and node itself using memory manager */
            /*scm_free(avl->scm, (void *) node->item);
            scm_free(avl->scm, node);*/

            /* Update AVL tree metadata */
            --avl->state->items;
            --avl->state->unique;
            return child;
        }

        /* Case with two children: find the smallest node in the right subtree */
        min = find_next_min_node(node->right);
        if (!min) {
            fprintf(stderr, "Error: Could not find minimum node in right subtree\n");
            return NULL;
        }

        /* Replace node's item with the minimum node's item */
        node->item = scm_strdup(avl->scm, min->item);
        if (!node->item) {
            fprintf(stderr, "Error: scm_strdup failed\n");
            return NULL;
        }

        /* Copy the count of the min node to the current node */
        node->count = min->count;

        /* Remove the minimum node from the right subtree */
        node->right = remove_min(node->right);

        /* Free memory for the minimum node's item and the minimum node itself */
        /*scm_free(avl->scm, (void *) min->item);
        scm_free(avl->scm, min);*/

        /* Update AVL tree metadata */
        --avl->state->items;
        --avl->state->unique;
    } 
    else if (cmp_result < 0) {
        /* If item is less, search in the left subtree */
        node->left = remove_node(avl, node->left, item);
    } 
    else {
        /* If item is greater, search in the right subtree */
        node->right = remove_node(avl, node->right, item);
    }

    /* Rebalance if needed */
    if (1 < abs(balance(node))) {
        if (balance(node->right) > 0) {
            node = rotate_right_left(node);
        } else {
            node = rotate_left(node);
        }
    }

    /* Update node depth after removal */
    node->depth = depth(node->left, node->right);
    return node;
}

/* Removes an item from the AVL tree */
int avl_remove(struct avl *avl, const char *item) {
    /* Validate inputs */
    assert(avl);
    assert(item && *item);  /* item should not be NULL or empty */

    /* Check if tree is empty */
    if (!avl->state->root) {
        fprintf(stderr, "AVL tree is empty!\n");
        return -1;
    }

    /* Remove the node and update the root */
    avl->state->root = remove_node(avl, avl->state->root, item);
    return 0;
}

uint64_t
avl_exists(const struct avl *avl, const char *item)
{
	const struct node *node;
	int d;

	assert( avl );
	assert( safe_strlen(item) );

	node = avl->state->root;
	while (node) {
		if (!(d = strcmp(item, node->item))) {
			return node->count;
		}
		node = (0 > d) ? node->left : node->right;
	}
	return 0;
}

void
avl_traverse(const struct avl *avl, avl_fnc_t fnc, void *arg)
{
	assert( avl );
	assert( fnc );

	traverse(avl->state->root, fnc, arg);
}

uint64_t
avl_items(const struct avl *avl)
{
	assert( avl );

	return avl->state->items;
}

uint64_t
avl_unique(const struct avl *avl)
{
	assert( avl );

	return avl->state->unique;
}

size_t
avl_scm_utilized(const struct avl *avl)
{
	assert( avl );

	return scm_utilized(avl->scm);
}

size_t
avl_scm_capacity(const struct avl *avl)
{
	assert( avl );

	return scm_capacity(avl->scm);
}
