
#include "stdafx.h"
#include "terrainquadtree.h"

using namespace graphic;

// Quad Tree Traverse Stack Memory (no multithread safe!!)
struct sData
{
	sRectf rect;
	int level;
	sQuadTreeNode<cTerrainQuadTree::sQuadData> *node;
};
sData g_stack[1024];


cTerrainQuadTree::cTerrainQuadTree()
	: m_isShowQuadTree(false)
	, m_isShowTexture(true)
	, m_isLight(true)
	, m_showQuadCount(0)
	, m_isShowLevel(false)
{
}

cTerrainQuadTree::cTerrainQuadTree(sRectf &rect)
{
}

cTerrainQuadTree::~cTerrainQuadTree()
{
}


bool cTerrainQuadTree::Create(graphic::cRenderer &renderer)
{
	sVertexNormTex vtx;
	vtx.p = Vector3(0, 0, 0);
	vtx.n = Vector3(0, 1, 0);
	vtx.u = 0;
	vtx.v = 0;
	m_vtxBuff.Create(renderer, 1, sizeof(sVertexNormTex), &vtx);

	if (!m_shader.Create(renderer, "../media/shader11/tess-pos-norm-tex.fxo", "Unlit"
		, eVertexType::POSITION | eVertexType::NORMAL | eVertexType::TEXTURE0))
	{
		return false;
	}

	m_mtrl.InitWhite();

	// root node 10 x 5 tiles
	//m_quadTree.m_rect = sRectf::Rect(0, 0, 4096, 4096);
	sRectf rootRect = sRectf::Rect(0, 0, 1 << cQuadTree<sQuadData>::MAX_LEVEL, 1 << cQuadTree<sQuadData>::MAX_LEVEL);
	m_qtree.m_rootRect = sRectf::Rect(0, 0, rootRect.right*10, rootRect.bottom*5);

	m_text.Create(renderer, 18.f, true, "Consolas");

	return true;
}


// Read HeightMap, write to texture
bool cTerrainQuadTree::ReadHeightMap(graphic::cRenderer &renderer, const char *fileName
	, const int bytesSize)
{
	const int fileSize = (int)common::FileSize(fileName);
	RETV(fileSize <= 0, false);
	const int vtxCnt = (int)sqrt(fileSize / 2);
	RETV(vtxCnt <= 0, false);

	std::ifstream ifs(fileName, std::ios::binary);
	if (!ifs.is_open())
		return false;

	BYTE *pdata = new BYTE[vtxCnt * vtxCnt * bytesSize];
	ZeroMemory(pdata, vtxCnt * vtxCnt * bytesSize);
	ifs.read((char*)pdata, vtxCnt * vtxCnt * bytesSize);

	float *map = new float[vtxCnt * vtxCnt];
	ZeroMemory(map, vtxCnt * vtxCnt * sizeof(float));

	for (int i = 0; i < vtxCnt * vtxCnt; i++)
		map[i] = (float)(*(short*)&pdata[i*bytesSize]) * 0.0001f;

	const bool result = m_heightTex.Create(renderer, vtxCnt, vtxCnt
		, DXGI_FORMAT_R32_FLOAT
		, map, vtxCnt * sizeof(float));
	
	delete[] pdata;
	delete[] map;
	return false;
}


void cTerrainQuadTree::Render(graphic::cRenderer &renderer
	, const graphic::cFrustum &frustum
	, const int limitLevel, const int level
	, const Ray &ray 
	, const Ray &mouseRay
)
{
	BuildQuadTree(frustum, ray);
	CalcSeamlessLevel();
	CalcSeamlessLevel();

	if (m_isShowTexture)
		RenderTessellation(renderer, frustum);

	if (m_isShowQuadTree)
		RenderQuad(renderer, frustum, mouseRay);
}


void cTerrainQuadTree::BuildQuadTree(const graphic::cFrustum &frustum
	, const Ray &ray)
{
	m_qtree.Clear();
	const Plane ground(Vector3(0, 1, 0), 0);
	const Vector3 rayPos = ground.Pick(ray.orig, ray.dir);

	for (int x = 0; x < 10; ++x)
	{
		for (int y = 0; y < 5; ++y)
		{
			sQuadTreeNode<sQuadData> *node = new sQuadTreeNode<sQuadData>;
			node->xLoc = x;
			node->yLoc = y;
			node->level = 0;
			m_qtree.Insert(node);
		}
	}

	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		sRectf rect = m_qtree.GetNodeRect(node);
		g_stack[sp++] = { rect, 0, node };
	}

	while (sp > 0)
	{
		const sRectf rect = g_stack[sp - 1].rect;
		const int lv = g_stack[sp - 1].level;
		sQuadTreeNode<sQuadData> *parentNode = g_stack[sp - 1].node;
		--sp;

		if (!IsContain(frustum, rect))
			continue;

		const float hw = rect.Width() / 2.f;
		const float hh = rect.Height() / 2.f;
		const Vector3 center = Vector3(rect.Center().x, 0, rect.Center().y);
		const float d1 = max(0, center.Distance(rayPos));
		const float distance = sqrt(d1*d1 + frustum.m_pos.y*frustum.m_pos.y);
		const int curLevel = GetLevel(distance);
		const bool isMinSize = hw < 1.f;

		if ((isMinSize || (lv >= curLevel)))
			continue;

		m_qtree.InsertChildren(parentNode);

		g_stack[sp++] = { sRectf::Rect(rect.left, rect.top, hw, hh), lv+1, parentNode->children[0] };
		g_stack[sp++] = { sRectf::Rect(rect.left + hw, rect.top, hw, hh), lv+1, parentNode->children[1] };
		g_stack[sp++] = { sRectf::Rect(rect.left, rect.top + hh, hw, hh), lv+1, parentNode->children[2] };
		g_stack[sp++] = { sRectf::Rect(rect.left + hw, rect.top + hh, hw, hh), lv+1, parentNode->children[3] };

		assert(sp < 1024);
	}
}


void cTerrainQuadTree::RenderTessellation(graphic::cRenderer &renderer
	, const graphic::cFrustum &frustum )
{
	m_shader.SetTechnique(m_isLight? "Light" : "Unlit");
	m_shader.Begin();
	m_shader.BeginPass(renderer, 0);

	renderer.m_cbLight.Update(renderer, 1);
	renderer.m_cbMaterial = m_mtrl.GetMaterial();
	renderer.m_cbMaterial.Update(renderer, 2);
	m_heightTex.Bind(renderer, 8);

	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		sRectf rect = m_qtree.GetNodeRect(node);
		g_stack[sp++] = { rect, 0, node };
	}

	while (sp > 0)
	{
		sQuadTreeNode<sQuadData> *node = g_stack[sp - 1].node;
		--sp;

		const sRectf rect = m_qtree.GetNodeRect(node);
		if (!IsContain(frustum, rect))
			continue;

		// leaf node?
		if (!node->children[0])
		{
			cQuadTile *tile = m_tileMgr.GetTile(renderer, *this, node->level, node->xLoc, node->yLoc, rect);
			tile->Render(renderer, node->data.level);
			node->data.tile = tile;
		}
		else
		{
			const sRectf rect = m_qtree.GetNodeRect(node);
			cQuadTile *tile = m_tileMgr.GetTile(renderer, *this, node->level, node->xLoc, node->yLoc, rect);
			node->data.tile = tile;

			for (int i = 0; i < 4; ++i)
				if (node->children[i])
					g_stack[sp++].node = node->children[i];
		}
	}

	renderer.UnbindShaderAll();
}


void cTerrainQuadTree::RenderQuad(graphic::cRenderer &renderer
	, const graphic::cFrustum &frustum
	, const Ray &ray)
{
	cShader11 *shader = renderer.m_shaderMgr.FindShader(eVertexType::POSITION);
	assert(shader);
	shader->SetTechnique("Unlit");
	shader->Begin();
	shader->BeginPass(renderer, 0);

	struct sInfo
	{
		sRectf r;
		sQuadTreeNode<sQuadData> *node;
	};
	sInfo showRects[512];
	int showRectCnt = 0;
	vector<std::pair<sRectf, cColor>> ars;
	m_showQuadCount = 0;

	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		sRectf rect = m_qtree.GetNodeRect(node);
		g_stack[sp++] = { rect, 0, node };
	}

	// Render Quad-Tree
	while (sp > 0)
	{
		sQuadTreeNode<sQuadData> *node = g_stack[sp - 1].node;
		--sp;

		// leaf node?
		if (!node->children[0])
		{
			const sRectf rect = m_qtree.GetNodeRect(node);
			const bool isShow = IsContain(frustum, rect);
			RenderRect3D(renderer, rect, cColor::BLACK);

			if (isShow && (showRectCnt < ARRAYSIZE(showRects)))
			{
				showRects[showRectCnt].r = rect;
				showRects[showRectCnt].node = node;
				showRectCnt++;
			}

			if (isShow)
				++m_showQuadCount;

			Plane ground(Vector3(0, 1, 0), 0);
			Vector3 pos = ground.Pick(ray.orig, ray.dir);
			if (sQuadTreeNode<sQuadData> *node = m_qtree.GetNode(sRectf::Rect(pos.x, pos.z, 0, 0)))
			{
				const sRectf rect = m_qtree.GetNodeRect(node);
				ars.push_back({ rect, cColor::WHITE });

				if (sQuadTreeNode<sQuadData> *north = m_qtree.GetNorthNeighbor(node))
				{
					const sRectf r = m_qtree.GetNodeRect(north);
					ars.push_back({ r, cColor::RED });
				}

				if (sQuadTreeNode<sQuadData> *east = m_qtree.GetEastNeighbor(node))
				{
					const sRectf r = m_qtree.GetNodeRect(east);
					ars.push_back({ r, cColor::GREEN });
				}

				if (sQuadTreeNode<sQuadData> *south = m_qtree.GetSouthNeighbor(node))
				{
					const sRectf r = m_qtree.GetNodeRect(south);
					ars.push_back({ r, cColor::BLUE });
				}

				if (sQuadTreeNode<sQuadData> *west = m_qtree.GetWestNeighbor(node))
				{
					const sRectf r = m_qtree.GetNodeRect(west);
					ars.push_back({ r, cColor::YELLOW });
				}
			}
		}
		else
		{
			for (int i = 0; i < 4; ++i)
				if (node->children[i])
					g_stack[sp++].node = node->children[i];
		}
	}

	// Render Show QuadNode
	for (int i = 0; i < showRectCnt; ++i)
	{
		const sRectf &rect = showRects[i].r;
		RenderRect3D(renderer, rect, cColor::WHITE);
	}

	// Render north, south, west, east quad node
	for (auto &data : ars)
	{
		const sRectf &r = data.first;

		CommonStates state(renderer.GetDevice());
		renderer.GetDevContext()->RSSetState(state.CullNone());
		renderer.GetDevContext()->OMSetDepthStencilState(state.DepthNone(), 0);
		RenderRect3D(renderer, r, data.second);
		renderer.GetDevContext()->OMSetDepthStencilState(state.DepthDefault(), 0);
		renderer.GetDevContext()->RSSetState(state.CullCounterClockwise());
	}

	// Render Show QuadNode
	if (m_isShowLevel)
	{
		for (int i = 0; i < showRectCnt; ++i)
		{
			const sRectf &rect = showRects[i].r;
			const int key = m_qtree.MakeKey(showRects[i].node->level, showRects[i].node->xLoc, showRects[i].node->yLoc);

			WStr32 str;
			str.Format(L"%d", showRects[i].node->level);
			m_text.SetColor(cColor::RED);
			m_text.SetText(str.c_str());

			const Vector3 offset = Vector3(rect.Width()*0.1f, 0, -rect.Height()*0.1f);
			const Vector2 pos = GetMainCamera().GetScreenPos(Vector3(rect.left, 0, rect.bottom) + offset);
			m_text.Render(renderer, pos.x, pos.y);
		}
	}
}


void cTerrainQuadTree::RenderRect3D(graphic::cRenderer &renderer
	, const sRectf &rect
	, const cColor &color
)
{
	Vector3 lines[] = {
		Vector3(rect.left, 0, rect.top)
		, Vector3(rect.right, 0, rect.top)
		, Vector3(rect.right, 0, rect.bottom)
		, Vector3(rect.left, 0, rect.bottom)
		, Vector3(rect.left, 0, rect.top)
	};

	renderer.m_rect3D.SetRect(renderer, lines, 5);

	renderer.m_cbPerFrame.m_v->mWorld = XMIdentity;
	renderer.m_cbPerFrame.Update(renderer);
	const Vector4 c = color.GetColor();
	renderer.m_cbMaterial.m_v->diffuse = XMVectorSet(c.x, c.y, c.z, c.w);
	renderer.m_cbMaterial.Update(renderer, 2);
	renderer.m_rect3D.m_vtxBuff.Bind(renderer);
	renderer.GetDevContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	renderer.GetDevContext()->DrawInstanced(renderer.m_rect3D.m_lineCount, 1, 0, 0);
}


// 다른 레벨의 쿼드가 인접해 있을 때, 테셀레이션을 이용해서, 마주보는 엣지의 버텍스 갯수를 맞춰준다.
// 맞닿아 있는 쿼드의 외곽 테셀레이션 계수를 동일하게 해서, 이음새가 생기지 않게 한다.
// When other levels of quads are adjacent, use tessellation to match the number of vertices in the facing edge.
// Make the outer tessellation coefficients of the quads equal, so that no seams occur.
void cTerrainQuadTree::CalcSeamlessLevel()
{
	int sp = 0;
	for (auto &node : m_qtree.m_roots)
	{
		sRectf rect = m_qtree.GetNodeRect(node);
		g_stack[sp++] = { rect, 15, node };
	}

	while (sp > 0)
	{
		sQuadTreeNode<sQuadData> *node = g_stack[sp - 1].node;
		--sp;

		// leaf node?
		if (!node->children[0])
		{
			if (sQuadTreeNode<sQuadData> *north = m_qtree.GetNorthNeighbor(node))
				CalcQuadEdgeLevel(node, north, eDirection::NORTH);
			else
				node->data.level[eDirection::NORTH] = node->level;

			if (sQuadTreeNode<sQuadData> *east = m_qtree.GetEastNeighbor(node))
				CalcQuadEdgeLevel(node, east, eDirection::EAST);
			else
				node->data.level[eDirection::EAST] = node->level;

			if (sQuadTreeNode<sQuadData> *south = m_qtree.GetSouthNeighbor(node))
				CalcQuadEdgeLevel(node, south, eDirection::SOUTH);
			else
				node->data.level[eDirection::SOUTH] = node->level;

			if (sQuadTreeNode<sQuadData> *west = m_qtree.GetWestNeighbor(node))
				CalcQuadEdgeLevel(node, west, eDirection::WEST);
			else
				node->data.level[eDirection::WEST] = node->level;
		}
		else
		{
			for (int i = 0; i < 4; ++i)
				if (node->children[i])
					g_stack[sp++].node = node->children[i];
		}
	}
}


// 다른 쿼드와 맞물릴 때, 높은 레벨의 쿼드를 배열에 저장한다.
// 테셀레이션에서, 자신의 레벨과, 엣지의 레벨을 비교해, 테셀레이션 팩터를 계산한다.
// Save high-level quads to the array when they are mingled with other quads.
// In tessellation, compute the tessellation factor by comparing its level with the level of the edge.
void cTerrainQuadTree::CalcQuadEdgeLevel(sQuadTreeNode<sQuadData> *from, sQuadTreeNode<sQuadData> *to
	, const eDirection::Enum dir)
{
	eDirection::Enum oppDir = GetOpposite(dir);

	if (from->data.level[dir] < from->level)
		from->data.level[dir] = from->level;

	if (to->data.level[oppDir] < from->data.level[dir])
		to->data.level[oppDir] = from->data.level[dir];

	if (from->data.level[dir] < to->data.level[oppDir])
		from->data.level[dir] = to->data.level[oppDir];
}


cTerrainQuadTree::eDirection::Enum cTerrainQuadTree::GetOpposite(const eDirection::Enum type)
{
	switch (type)
	{
	case eDirection::NORTH: return eDirection::SOUTH;
	case eDirection::EAST: return eDirection::WEST;
	case eDirection::SOUTH: return eDirection::NORTH;
	case eDirection::WEST: return eDirection::EAST;
	default:
		assert(0);
		return eDirection::SOUTH;
	}
}


// frustum 안에 rect가 포함되면 true를 리턴한다.
inline bool cTerrainQuadTree::IsContain(const graphic::cFrustum &frustum, const sRectf &rect)
{
	const float hw = rect.Width() / 2.f;
	const float hh = rect.Height() / 2.f;
	const Vector3 center = Vector3(rect.Center().x, 0, rect.Center().y);
	cBoundingBox bbox;
	bbox.SetBoundingBox(center + Vector3(0,hh,0), Vector3(hw, hh, hh), Quaternion());
	const bool reval = frustum.IsInBox(bbox);
	return reval;
}


// 카메라와 거리에 따라, 쿼드 레벨을 계산한다.
inline int cTerrainQuadTree::GetLevel(const float distance)
{
	int dist = (int)distance - 10;

#define CALC(lv) if ((dist >>= 1) < 1) return lv;

	CALC(cQuadTree<sQuadData>::MAX_LEVEL);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 1);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 2);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 3);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 4);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 5);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 6);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 7);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 8);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 9);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 10);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 11);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 12);
	CALC(cQuadTree<sQuadData>::MAX_LEVEL - 13);
	return 0;
}
