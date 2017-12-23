//
// 2017-12-23, jjuiddong
// Quad-Tree for Geometry (Sparse QuadTree)
//
//- Game Programming Gems2, 4.5
//	- Direct Access QuadTree Lookup
//
#pragma once


template<class T>
struct sQuadTreeNode : public common::cMemoryPool<sQuadTreeNode<T>>
{
	int xLoc;
	int yLoc;
	int level;
	sQuadTreeNode *parent; // reference
	sQuadTreeNode *children[4]; // reference
	T data;

	sQuadTreeNode() {
		xLoc = 0;
		yLoc = 0;
		level = 0;
		parent = NULL;
		children[0] = children[1] = children[2] = children[3] = NULL;
	}
};


template<class T>
class cQuadTree
{
public:
	cQuadTree();
	~cQuadTree();

	bool Insert(sQuadTreeNode<T> *node);
	bool Remove(sQuadTreeNode<T> *node, const bool isRmTree = true);
	bool InsertChildren(sQuadTreeNode<T> *node);
	bool RemoveChildren(sQuadTreeNode<T> *node, const bool isRmTree = true);
	sQuadTreeNode<T>* GetNode(const sRectf &rect);
	sQuadTreeNode<T>* GetNode(const int level, const int xLoc, const int yLoc);
	sQuadTreeNode<T>* GetNorthNeighbor(sQuadTreeNode<T> *node);
	sQuadTreeNode<T>* GetSouthNeighbor(sQuadTreeNode<T> *node);
	sQuadTreeNode<T>* GetWestNeighbor(sQuadTreeNode<T> *node);
	sQuadTreeNode<T>* GetEastNeighbor(sQuadTreeNode<T> *node);
	sRectf GetNodeRect(sQuadTreeNode<T> *node);
	inline int MakeKey(const int level, const int xLoc, const int yLoc);
	void Clear();


public:
	enum {MAX_LEVEL = 14};

	vector<sQuadTreeNode<T>*> m_roots; // root nodes (multiple root node)
	sRectf m_rootRect;
	const float m_quadScale = 1.f;
	std::map<int, sQuadTreeNode<T>*> m_nodeTable[MAX_LEVEL]; // level quad tree map
														  // key = quad node x,y index (linear)
};


// m_nodeTable key
// Level            Key
//   1               0
//   2       0       |        1       |       2      |        3
//   3    0,1,2,3    |    4,5,6,7     |  8,9,10,11   |   12,13,14,15
//   4    ....

template<class T>
inline cQuadTree<T>::cQuadTree()
{
	m_rootRect = sRectf::Rect(0, 0, 1 << MAX_LEVEL, 1 << MAX_LEVEL);
}

template<class T>
inline cQuadTree<T>::~cQuadTree()
{
	Clear();
}


template<class T>
inline int cQuadTree<T>::MakeKey(const int level, const int xLoc, const int yLoc)
{
	return ((yLoc << level) * 10) + xLoc;
}


template<class T>
inline bool cQuadTree<T>::Insert(sQuadTreeNode<T> *node)
{
	RETV(node->level >= MAX_LEVEL, false);

	const int key = MakeKey(node->level, node->xLoc, node->yLoc);

	auto it = m_nodeTable[node->level].find(key);
	if (m_nodeTable[node->level].end() != it)
		return false; // Error!! Already Exist

	m_nodeTable[node->level][key] = node;

	if (node->level == 0)
		m_roots.push_back(node);

	return true;
}


template<class T>
inline bool cQuadTree<T>::Remove(sQuadTreeNode<T> *node
	, const bool isRmTree //= true
)
{
	RETV(node->level >= MAX_LEVEL, false);

	const int key = MakeKey(node->level, node->xLoc, node->yLoc);

	auto it = m_nodeTable[node->level].find(key);
	if (m_nodeTable[node->level].end() == it)
		return false; // Error!! Not Exist

	for (int i = 0; i < 4; ++i)
		if (node->children[i])
			Remove(node->children[i], isRmTree);

	m_nodeTable[node->level].erase(key);

	if (isRmTree)
		delete node;

	return true;
}


// node의 chilren에 node를 추가한다.
template<class T>
inline bool cQuadTree<T>::InsertChildren(sQuadTreeNode<T> *node)
{
	RETV((node->level + 1) >= MAX_LEVEL, false);

	int locs[] = { 0,0, 1,0, 0,1, 1,1 }; //x,y loc

	for (int i = 0; i < 4; ++i)
	{
		sQuadTreeNode<T> *p = new sQuadTreeNode<T>;
		p->xLoc = (node->xLoc << 1) + locs[i * 2];
		p->yLoc = (node->yLoc << 1) + locs[i * 2 + 1];
		p->level = node->level + 1;
		p->parent = node;
		node->children[i] = p;
		Insert(p);
	}

	return true;
}


template<class T>
inline bool cQuadTree<T>::RemoveChildren(sQuadTreeNode<T> *node
	, const bool isRmTree //= true
)
{
	for (int i = 0; i < 4; ++i)
	{
		if (node->children[i])
		{
			Remove(node->children[i], isRmTree);
			node->children[i] = NULL;
		}
	}
	return true;
}


template<class T>
inline sQuadTreeNode<T>* cQuadTree<T>::GetNode(const sRectf &rect)
{
	if ((rect.left > m_rootRect.right) || (rect.top > m_rootRect.bottom)
		|| (rect.right < 0) || (rect.bottom < 0))
		return NULL;

	int x1 = (int)(rect.left * m_quadScale);
	int y1 = (int)(rect.top * m_quadScale);

	int xResult = x1 ^ ((int)(rect.right * m_quadScale));
	int yResult = y1 ^ ((int)(rect.bottom * m_quadScale));

	int nodeLevel = MAX_LEVEL;
	int shiftCount = 0;

	while (xResult + yResult != 0)
	{
		xResult >>= 1;
		yResult >>= 1;
		nodeLevel--;
		shiftCount++;
	}

	x1 >>= shiftCount;
	y1 >>= shiftCount;

	sQuadTreeNode<T> *ret = NULL;
	do
	{
		ret = GetNode(nodeLevel, x1, y1);
		if (nodeLevel <= 0)
			break;

		// goto parent level
		x1 >>= 1;
		y1 >>= 1;
		--nodeLevel;
	} while (ret == NULL);

	return ret;
}


template<class T>
inline sQuadTreeNode<T>* cQuadTree<T>::GetNode(const int level, const int xLoc, const int yLoc)
{
	if ((u_int)level >= MAX_LEVEL)
		return NULL;

	const int key = MakeKey(level, xLoc, yLoc);

	auto it = m_nodeTable[level].find(key);
	if (m_nodeTable[level].end() == it)
		return false; // Error!! Not Exist

	return m_nodeTable[level][key];
}


template<class T>
inline sQuadTreeNode<T>* cQuadTree<T>::GetNorthNeighbor(sQuadTreeNode<T> *node)
{
	sRectf rect = GetNodeRect(node);
	Vector2 pos = rect.Center();
	pos.y += (rect.Height() / 2.f + (m_quadScale / 2.f));

	sQuadTreeNode<T> *ret = GetNode(sRectf::Rect(pos.x, pos.y, 0, 0));
	return (ret == node) ? NULL : ret;
}


template<class T>
inline sQuadTreeNode<T>* cQuadTree<T>::GetSouthNeighbor(sQuadTreeNode<T> *node)
{
	sRectf rect = GetNodeRect(node);
	Vector2 pos = rect.Center();
	pos.y -= (rect.Height() / 2.f + (m_quadScale / 2.f));

	sQuadTreeNode<T> *ret = GetNode(sRectf::Rect(pos.x, pos.y, 0, 0));
	return (ret == node) ? NULL : ret;
}


template<class T>
inline sQuadTreeNode<T>* cQuadTree<T>::GetWestNeighbor(sQuadTreeNode<T> *node)
{
	sRectf rect = GetNodeRect(node);
	Vector2 pos = rect.Center();
	pos.x -= (rect.Width() / 2.f + (m_quadScale / 2.f));

	sQuadTreeNode<T> *ret = GetNode(sRectf::Rect(pos.x, pos.y, 0, 0));
	return (ret == node) ? NULL : ret;
}


template<class T>
inline sQuadTreeNode<T>* cQuadTree<T>::GetEastNeighbor(sQuadTreeNode<T> *node)
{
	sRectf rect = GetNodeRect(node);
	Vector2 pos = rect.Center();
	pos.x += (rect.Width() / 2.f + (m_quadScale / 2.f));

	sQuadTreeNode<T> *ret = GetNode(sRectf::Rect(pos.x, pos.y, 0, 0));
	return (ret == node) ? NULL : ret;
}


template<class T>
inline sRectf cQuadTree<T>::GetNodeRect(sQuadTreeNode<T> *node)
{
	const float size = (float)(1 << (MAX_LEVEL - node->level)) * m_quadScale;
	const sRectf rect = sRectf::Rect(node->xLoc * size, node->yLoc*size, size, size);
	return rect;
}


template<class T>
inline void cQuadTree<T>::Clear()
{
	for (int i = 0; i < MAX_LEVEL; ++i)
	{
		for (auto kv : m_nodeTable[i])
			delete kv.second;
		m_nodeTable[i].clear();
	}
	m_roots.clear();
}
