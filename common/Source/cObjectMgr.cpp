#include "agk.h"
//#include "Wrapper.h"

using namespace AGK;

cCamera* cObjectMgr::g_pCurrentCamera = 0;

cObjectMgr::cObjectMgr()
{
	m_pOpaqueObjects = UNDEF;
	m_pLastOpaque = UNDEF;
	m_pAlphaObjects = UNDEF;
	m_iNumAlphaObjects = 0;
	m_pAlphaObjectsArray = 0;

	iCurrentCount = 0;
	m_iLastSorted = 0; 
	m_iLastDrawn = 0; 
	m_iLastTotal = 0;
	m_iLastDrawCalls = 0;

	m_bSortDepth = true;
	m_pCurrentShader = 0;

	m_pSkyBox = 0;
}

cObjectMgr::~cObjectMgr()
{
	ClearAll();
}

void cObjectMgr::ClearAll()
{
	cObjectContainer *pCont;
	
	while ( m_pOpaqueObjects )
	{
		pCont = m_pOpaqueObjects;
		m_pOpaqueObjects = m_pOpaqueObjects->m_pNext;
		delete pCont;
	}
	m_pOpaqueObjects = NULL;
	m_pLastOpaque = NULL;

	while ( m_pAlphaObjects )
	{
		pCont = m_pAlphaObjects;
		m_pAlphaObjects = m_pAlphaObjects->m_pNext;
		delete pCont;
	}
	m_pAlphaObjects = NULL;

	iCurrentCount = 0;
	m_bSortDepth = true;
}

void cObjectMgr::SetSortDepth( bool sort )
{
	m_bSortDepth = sort;
}

void cObjectMgr::AddObject( cObject3D* object )
{
	if ( !object ) return;

	cObjectContainer *pNewMember = new cObjectContainer();
	pNewMember->SetObject( object );
	object->m_iObjFlags |= AGK_OBJECT_MANAGED;

	if ( !AddContainer( pNewMember ) ) delete pNewMember;
}

bool cObjectMgr::AddContainer( cObjectContainer* pNewMember )
{
	if ( !pNewMember ) return false;
	if ( pNewMember->GetType() == 0 ) return false;

	pNewMember->m_pNext = UNDEF;
	
	int iMode = pNewMember->GetDrawMode();
	if ( iMode == 0 )
	{
		// opaque, add to the end of the list to draw in order they were added
		pNewMember->m_pNext = 0;;
		if ( m_pLastOpaque ) m_pLastOpaque->m_pNext = pNewMember;
		else m_pOpaqueObjects = pNewMember;
		m_pLastOpaque = pNewMember;
	}
	else
	{
		// alpha transparency, should be drawn by depth back to front
		pNewMember->m_pNext = m_pAlphaObjects;
		m_pAlphaObjects = pNewMember;
	}

	return true;
}

void cObjectMgr::RemoveObject( cObject3D* object )
{
	if ( !object ) return;
	object->m_iObjFlags &= ~AGK_OBJECT_MANAGED;

	// need to check all arrays
	// opaque objects
	cObjectContainer *pMember = m_pOpaqueObjects;
	cObjectContainer *pLast = UNDEF;
	while ( pMember )
	{
		if ( pMember->GetType() == 1 && pMember->GetObject() == object )
		{
			//found, remove it
			cObjectContainer *pNext = pMember->m_pNext;

			if ( m_pLastOpaque == pMember ) m_pLastOpaque = pLast;
			
			if ( pLast ) pLast->m_pNext = pNext;
			else m_pOpaqueObjects = pNext;

			delete pMember;
			pMember = pNext;
			continue;
		}

		pLast = pMember;
		pMember = pMember->m_pNext;
	}

	for ( int i = 0; i < m_iNumAlphaObjects; i++ )
	{
		if ( m_pAlphaObjectsArray[ i ] && m_pAlphaObjectsArray[ i ]->GetType() == 1 && m_pAlphaObjectsArray[ i ]->GetObject() == object ) 
		{
			m_pAlphaObjectsArray[ i ] = 0;
			break;
		}
	}

	// alpha objects
	pLast = UNDEF;
	pMember = m_pAlphaObjects;
	while ( pMember )
	{
		if ( pMember->GetType() == 1 && pMember->GetObject() == object )
		{
			//found, remove it
			cObjectContainer *pNext = pMember->m_pNext;
			
			if ( pLast ) pLast->m_pNext = pNext;
			else m_pAlphaObjects = pNext;

			delete pMember;
			pMember = pNext;
			continue;
		}

		pLast = pMember;
		pMember = pMember->m_pNext;
	}
}

void cObjectMgr::UpdateAll( float time )
{
	cObjectContainer *pMember = m_pOpaqueObjects;
	while ( pMember )
	{
		if ( pMember->GetType() == 1 ) pMember->GetObject()->Update( time );
		pMember = pMember->m_pNext;
	}

	pMember = m_pAlphaObjects;
	while ( pMember )
	{
		if ( pMember->GetType() == 1 ) pMember->GetObject()->Update( time );
		pMember = pMember->m_pNext;
	}
}

void cObjectMgr::ResortAll()
{
	m_iLastTotal = 0;

	// check for any changed objects that need reordering
	// check back opaque objects
	cObjectContainer *pMember = m_pOpaqueObjects;
	cObjectContainer *pLast = UNDEF;
	cObjectContainer *pChanged = UNDEF;
	cObjectContainer *pLastChanged = UNDEF;
	while ( pMember )
	{
		if ( pMember->GetType() == 1 ) m_iLastTotal++;
		
		if ( pMember->GetTransparencyChanged() )
		{
			// object has changed, remove it from this list
			cObjectContainer *pNext = pMember->m_pNext;
			if ( pLast ) pLast->m_pNext = pNext;
			else m_pOpaqueObjects = pNext;

			if ( m_pLastOpaque == pMember ) m_pLastOpaque = pLast;

			// add it to the changed list, deal with it later
			pMember->m_pNext = 0;
			if ( pLastChanged ) pLastChanged->m_pNext = pMember;
			else pChanged = pMember;
			pLastChanged = pMember;

			pMember = pNext;
			continue;
		}

		pLast = pMember;
		pMember = pMember->m_pNext;
	}

	// check alpha objects
	pMember = m_pAlphaObjects;
	pLast = UNDEF;
	while ( pMember )
	{
		if ( pMember->GetType() == 1 ) m_iLastTotal++;
		
		if ( pMember->GetTransparencyChanged() )
		{
			// object has changed, remove it from this list
			cObjectContainer *pNext = pMember->m_pNext;
			if ( pLast ) pLast->m_pNext = pNext;
			else m_pAlphaObjects = pNext;

			// add it to the changed list, deal with it later
			pMember->m_pNext = 0;
			if ( pLastChanged ) pLastChanged->m_pNext = pMember;
			else pChanged = pMember;
			pLastChanged = pMember;

			pMember = pNext;
			continue;
		}

		pLast = pMember;
		pMember = pMember->m_pNext;
	}

	m_iLastSorted = 0;
	
	// re-add changed objects
	pMember = pChanged;
	cObjectContainer *pNext = UNDEF;
	while ( pMember )
	{
		pNext = pMember->m_pNext;
		if ( !AddContainer( pMember ) ) delete pMember;
		else m_iLastSorted++;
		pMember = pNext;
	}

	// count transparent objects
	int alphaCount = 0;
	pMember = m_pAlphaObjects;
	while ( pMember )
	{
		alphaCount++;
		pMember = pMember->m_pNext;
	}

	// resize transparent array if needed
	if ( alphaCount > m_iNumAlphaObjects )
	{
		if ( m_pAlphaObjectsArray ) delete [] m_pAlphaObjectsArray;
		m_pAlphaObjectsArray = 0;
		if ( alphaCount > 0 ) m_pAlphaObjectsArray = new cObjectContainer*[ alphaCount ];
	}

	m_iNumAlphaObjects = alphaCount;

	// rebuild transparent array
	alphaCount = 0;
	pMember = m_pAlphaObjects;
	cObject3D* tmpobj;
	while ( pMember )
	{
		m_pAlphaObjectsArray[ alphaCount ] = pMember;

		if (g_pCurrentCamera) {
			//PE: Store SqrDist so we dont need to update it while sorting.
			tmpobj = ((cObject3D*) pMember->GetObject()); 
			tmpobj->m_fLastSqrDist = tmpobj->posFinal().GetSqrDist(g_pCurrentCamera->posFinal());
		}

		alphaCount++;
		
		pMember = pMember->m_pNext;
	}

	// sort transparent objects
	// todo don't use qsort
	//PE: Poul , std::sort should be 30% faster, perhaps give that a go :)
//	if (m_pAlphaObjectsArray && g_pCurrentCamera) qsort(m_pAlphaObjectsArray, m_iNumAlphaObjects, sizeof(cObjectContainer*), cObjectMgr::ContainerCompare);
	if (m_pAlphaObjectsArray && g_pCurrentCamera) qsort(m_pAlphaObjectsArray, m_iNumAlphaObjects, sizeof(cObjectContainer*), cObjectMgr::ContainerComparePE);
}

//PE: ContainerCompare is called many many times, so store GetSqrDist so we dont need to do this everytime. It will not change while sorting.
//PE: With 722 objects, the old sort used 15FPS (difference with sort/nosort), using this we are down to only using 6 FPS for the same sort.
int cObjectMgr::ContainerComparePE( const void* a, const void* b )
{
	float dist1 = ((cObject3D*)(*(cObjectContainer**)a)->GetObject())->m_fLastSqrDist; //PE: used our stored distance.
	float dist2 = ((cObject3D*)(*(cObjectContainer**)b)->GetObject())->m_fLastSqrDist; //PE: used our stored distance.

	if ( dist2 == dist1 ) return 0;
	else if ( dist2 < dist1 ) return -1; // b is closer to the camera than a
	else return 1;
}

int cObjectMgr::ContainerCompare(const void* a, const void* b)
{
	//if ( !g_pCurrentCamera ) return 0; //PE: Already checked before qsort.

	cObject3D* obj1 = (*(cObjectContainer**)a)->GetObject();
	cObject3D* obj2 = (*(cObjectContainer**)b)->GetObject();

	float dist1 = obj1->posFinal().GetSqrDist( g_pCurrentCamera->posFinal() );
	float dist2 = obj2->posFinal().GetSqrDist( g_pCurrentCamera->posFinal() );

	if (dist2 == dist1) return 0;
	else if (dist2 < dist1) return -1; // b is closer to the camera than a
	else return 1;
}

void cObjectMgr::SetCurrentCamera( cCamera* pCamera )
{
	if ( g_pCurrentCamera == pCamera ) return;
	g_pCurrentCamera = pCamera;
}

void cObjectMgr::DrawAll()
{
	ResortAll();

	m_iLastDrawn = 0;
	m_iLastDrawCalls = 0;
	
	if ( m_pOpaqueObjects ) DrawList( m_pOpaqueObjects, 0 );
	// skybox drawn last using the depth buffer to cull as much as possible, but before transparent objects
	if ( m_pSkyBox ) m_pSkyBox->Draw();

	//if ( m_pAlphaObjects ) DrawList( m_pAlphaObjects, 1 );
	if ( m_pAlphaObjectsArray )
	{
		for ( int i = 0 ; i < m_iNumAlphaObjects; i++ )
		{
			if ( m_pAlphaObjectsArray[ i ] && m_pAlphaObjectsArray[ i ]->GetType() == 1 )
			{
				m_iLastDrawn++;
				m_pAlphaObjectsArray[ i ]->GetObject()->Draw();
			}
		}
	}
}

void cObjectMgr::DrawList( cObjectContainer *pList, int mode )
{
	if ( !pList ) return;

	cObjectContainer *pObject = pList;
	while ( pObject )
	{
		if ( pObject->GetType() == 1 )
		{
			m_iLastDrawn++;
			pObject->GetObject()->Draw();
		}
		pObject = pObject->m_pNext;
	}
}

void cObjectMgr::DrawShadowMap()
{
	if ( m_pOpaqueObjects ) DrawShadowList( m_pOpaqueObjects, 0 );
	if ( m_pAlphaObjects ) DrawShadowList( m_pAlphaObjects, 1 );
}

void cObjectMgr::DrawShadowList( cObjectContainer *pList, int mode )
{
	if ( !pList ) return;

	cObjectContainer *pObject = pList;
	while ( pObject )
	{
		if ( pObject->GetType() == 1 )
		{
			pObject->GetObject()->DrawShadow();
		}
		pObject = pObject->m_pNext;
	}
}
