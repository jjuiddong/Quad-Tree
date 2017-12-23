
#include "stdafx.h"
#include "quadtile.h"
#include "terrainquadtree.h"


using namespace graphic;

//--------------------------------------------------------------------
cQuadTile::cQuadTile()
	: m_texture(NULL)
	, m_replaceTex(NULL)
	, m_vtxBuff(NULL)
	, m_replaceTexLv(-1)
{
}

cQuadTile::~cQuadTile()
{
}


bool cQuadTile::Create(graphic::cRenderer &renderer, const int level, const int xLoc, const int yLoc
	, const sRectf &rect
	, const char *textureFileName)
{
	m_level = level;
	m_loc = Vector2i(xLoc, yLoc);
	m_rect = rect;
	
	m_uvs[0] = 0.f; // left-top u
	m_uvs[1] = 0.f; // left-top v
	m_uvs[2] = 1.f; // right-bottom u
	m_uvs[3] = 1.f; // right-bottom v

	return true;
}


// adjLevel: eDirection index, adjacent quad level
void cQuadTile::Render(graphic::cRenderer &renderer
	, const int adjLevel[4]
	, const XMMATRIX &tm //= graphic::XMIdentity
)
{
	Transform tfm;
	tfm.pos.x = m_rect.left;
	tfm.pos.z = m_rect.top;
	tfm.pos.y = 0.f;
	renderer.m_cbPerFrame.m_v->mWorld = XMMatrixTranspose(tfm.GetMatrixXM());
	renderer.m_cbPerFrame.Update(renderer);

	//  Z axis
	// /|\
	//  |
	//  |
	//  |
	// -----------> X axis
	//
	//	 Clock Wise Tessellation Quad
	//        North
	//  * ------1--------*
	//  |                |
	//  |                |
	//  0 West           2 East
	//  |                |
	//  |                |
	//  * ------3--------*
	//         South

	renderer.m_cbTessellation.m_v->size = Vector2(m_rect.Width(), m_rect.Height());
	renderer.m_cbTessellation.m_v->level = (float)m_level;
	renderer.m_cbTessellation.m_v->edgeLevel[0] = (float)adjLevel[cTerrainQuadTree::eDirection::WEST];
	renderer.m_cbTessellation.m_v->edgeLevel[1] = (float)adjLevel[cTerrainQuadTree::eDirection::NORTH];
	renderer.m_cbTessellation.m_v->edgeLevel[2] = (float)adjLevel[cTerrainQuadTree::eDirection::EAST];
	renderer.m_cbTessellation.m_v->edgeLevel[3] = (float)adjLevel[cTerrainQuadTree::eDirection::SOUTH];

	if (m_texture)
	{
		renderer.m_cbTessellation.m_v->uvs[0] = 0.f;
		renderer.m_cbTessellation.m_v->uvs[1] = 0.f;
		renderer.m_cbTessellation.m_v->uvs[2] = 1.f;
		renderer.m_cbTessellation.m_v->uvs[3] = 1.f;
		m_texture->Bind(renderer, 0);
	}
	else if (m_replaceTex)
	{
		renderer.m_cbTessellation.m_v->uvs[0] = m_uvs[0];
		renderer.m_cbTessellation.m_v->uvs[1] = m_uvs[1];
		renderer.m_cbTessellation.m_v->uvs[2] = m_uvs[2];
		renderer.m_cbTessellation.m_v->uvs[3] = m_uvs[3];
		m_replaceTex->Bind(renderer, 0);
	}
	else
	{
		ID3D11ShaderResourceView *ns[1] = { NULL };
		renderer.GetDevContext()->PSSetShaderResources(0, 1, ns);
	}

	renderer.m_cbTessellation.Update(renderer, 6);

	m_vtxBuff->Bind(renderer);
	renderer.GetDevContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
	renderer.GetDevContext()->Draw(m_vtxBuff->GetVertexCount(), 0);
}


//--------------------------------------------------------------------
cQuadTileManager::cQuadTileManager()
{
}

cQuadTileManager::~cQuadTileManager()
{
	Clear();
}


bool cQuadTileManager::Update(const float deltaSeconds)
{
	return true;
}


cQuadTile* cQuadTileManager::GetTile(graphic::cRenderer &renderer
	, cTerrainQuadTree &terrain
	, const int level, const int xLoc, const int yLoc
	, const sRectf &rect )
{
	if (!m_vtxBuff.m_vtxBuff)
	{
		sVertexNormTex vtx[4] = {
			{Vector3(0,0,1), Vector3(0,1,0), 0, 0}
			,{ Vector3(1,0,1), Vector3(0,1,0), 1, 0 }
			,{ Vector3(0,0,0), Vector3(0,1,0), 0, 1 }
			,{ Vector3(1,0,0), Vector3(0,1,0), 1, 1 }
		};
		m_vtxBuff.Create(renderer, 4, sizeof(sVertexNormTex), vtx);
	}

	const u_int key = (level * 1000000000) + (yLoc * 10000) + xLoc;
	auto it = m_tiles.find(key);
	if (m_tiles.end() != it)
		return it->second;

	StrPath fileName;
	fileName.Format("../media/WorldTerrain/BMNGWMS/%d/%d/%d_%d.dds", level, yLoc, yLoc, xLoc);

	cQuadTile *tile = new cQuadTile();
	tile->Create(renderer, level, xLoc, yLoc, rect, NULL);
	tile->m_vtxBuff = &m_vtxBuff;

	// 파일이 있다면, 텍스쳐를 로딩한다.
	if (fileName.IsFileExist())
	{
		cResourceManager::Get()->LoadTextureParallel(renderer, fileName);
		cResourceManager::Get()->AddParallelLoader(new cParallelLoader(cParallelLoader::eType::TEXTURE
			, fileName, (void**)&tile->m_texture));
	}
	else
	{
		dbg::Logp("Error Load Texture %s\n", fileName.c_str());
	}

	// 텍스쳐 파일이 없다면, 더 높은 레벨의 텍스쳐를 이용하고, uv를 재조정한다.
	// 텍스쳐 로딩이 병렬로 이뤄지기 때문에, 로딩이 이뤄지는 동안, 대체 텍스쳐를 이용한다.
	ReplaceParentTexture(terrain, tile, level, xLoc, yLoc, rect);
	m_tiles[key] = tile;

	return tile;
}


// 텍스쳐 파일이 없다면, 더 높은 레벨의 텍스쳐를 이용하고, uv를 재조정한다.
bool cQuadTileManager::ReplaceParentTexture(cTerrainQuadTree &terrain
	, cQuadTile *tile
	, const int level, const int xLoc, const int yLoc
	, const sRectf &rect)
{
	int lv = level;
	int x = xLoc;
	int y = yLoc;
	sQuadTreeNode<cTerrainQuadTree::sQuadData> *parentNode = NULL;

	// 부모 레벨의 노드를 찾는다.
	do
	{
		--lv;
		x >>= 1;
		y >>= 1;
		sQuadTreeNode<cTerrainQuadTree::sQuadData> *p = terrain.m_qtree.GetNode(lv, x, y);
		//if (p && p->data.tile && p->data.tile->m_texture && (p->data.tile->m_replaceTexLv == -1))
		if (p && p->data.tile && p->data.tile->m_texture)
			parentNode = p;

	} while ((lv > 0) && !parentNode);

	// 상위 레벨의 텍스쳐를 가져 온 후, uv 좌표를 보정한다.
	if (parentNode && parentNode->data.tile)
	{
		// rect 와 3d 상의 rect 는 z축이 뒤바뀐 상태다. 
		// 그래서, bottom 이 top 이되고, top 이 bottom 이 된다.
		// uv 계산시 발생하는 음수는 항상 양수로 계산하면 문제 없다.
		sRectf parentR = parentNode->data.tile->m_rect;
		const Vector2 lt = Vector2(parentR.left, parentR.bottom); // left-top
		const Vector2 rb = Vector2(parentR.right, parentR.top); // right-bottom
		const Vector2 clt = Vector2(rect.left, rect.bottom); // current rect left-top
		const Vector2 crb = Vector2(rect.right, rect.top); // current rect right-bottom

		const float w = parentR.Width();
		const float h = parentR.Height();
		const float u1 = (clt.x - lt.x) / w;
		const float v1 = (clt.y - lt.y) / h;
		const float u2 = (crb.x - lt.x) / w;
		const float v2 = (crb.y - lt.y) / h;
		tile->m_uvs[0] = abs(u1);
		tile->m_uvs[1] = abs(v1);
		tile->m_uvs[2] = abs(u2);
		tile->m_uvs[3] = abs(v2);
		tile->m_replaceTex = parentNode->data.tile->m_texture;
		tile->m_replaceTexLv = parentNode->level;
	}

	return true;
}


void cQuadTileManager::Clear()
{
	for (auto &kv : m_tiles)
		delete kv.second;
	m_tiles.clear();
}
