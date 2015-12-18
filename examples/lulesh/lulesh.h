#if !defined(USE_MPI)
# error "You should specify USE_MPI=0 or USE_MPI=1 on the compile line"
#endif


// OpenMP will be compiled in if this flag is set to 1 AND the compiler beging
// used supports it (i.e. the _OPENMP symbol is defined)
#define USE_OMP 1

#if USE_MPI
# include <mpi.h>
#endif

#include <math.h>
#include <vector>

#if _CD
#include "packer.h"
#include "unpacker.h"
#include "cd.h"
#include "serdes.h"
using namespace std;
#endif
//**************************************************
// Allow flexibility for arithmetic representations 
//**************************************************

#define MAX(a, b) ( ((a) > (b)) ? (a) : (b))

// CD
#define SERDES_ENABLED 1

#define BEG_CDMAPPING_FUNC(ENABLE, CHILD_NUM, child_num, ...) { \
#if ENABLE==2 \
  CREATE_AND_BEGIN(CHILD_NUM, 

#define CREATE_AND_BEGIN(CHILD_NUM, CD_TYPE,child_num, ...)  \
  string funcName(__func__); \
  fprintf(stdout, "%s\", funcName.c_str()); \
  CDHandle* cdh = GetLeafCD(); \
  CDHandle* child = cdh->Create(CHILD_NUM, (funcName+cdh->GetString()).c_str(), CD_TYPE); \
  CD_Begin(child, false, funcName.c_str()); \


#define END_CDMAPPING() { \
  cdh->Detect(); \
  CD_Complete(cdh); \
}


#define BEGIN_CDMAPPING(createNextLevel, child_num, ...) { \
  string funcName(__func__); \
  fprintf(stdout, "%s\", funcName.c_str()); \
  CDHandle* cdh = GetLeafCD(); \
  CD_Begin(cdh, false, funcName.c_str()); \
  if(createNextLevel) { \
    cdh->Create(child_num, string, kStrict, 0, 0, &err); \
  } \
}

#define END_CDMAPPING() { \
  cdh->Detect(); \
  CD_Complete(cdh); \
}


// Precision specification
typedef float        real4 ;
typedef double       real8 ;
typedef long double  real10 ;  // 10 bytes on x86

typedef int    Index_t ; // array subscript and loop index
typedef real8  Real_t ;  // floating point representation
typedef int    Int_t ;   // integer representation

enum { VolumeError = -1, QStopError = -2 } ;

inline real4  SQRT(real4  arg) { return sqrtf(arg) ; }
inline real8  SQRT(real8  arg) { return sqrt(arg) ; }
inline real10 SQRT(real10 arg) { return sqrtl(arg) ; }

inline real4  CBRT(real4  arg) { return cbrtf(arg) ; }
inline real8  CBRT(real8  arg) { return cbrt(arg) ; }
inline real10 CBRT(real10 arg) { return cbrtl(arg) ; }

inline real4  FABS(real4  arg) { return fabsf(arg) ; }
inline real8  FABS(real8  arg) { return fabs(arg) ; }
inline real10 FABS(real10 arg) { return fabsl(arg) ; }


// Stuff needed for boundary conditions
// 2 BCs on each of 6 hexahedral faces (12 bits)
#define XI_M        0x00007
#define XI_M_SYMM   0x00001
#define XI_M_FREE   0x00002
#define XI_M_COMM   0x00004

#define XI_P        0x00038
#define XI_P_SYMM   0x00008
#define XI_P_FREE   0x00010
#define XI_P_COMM   0x00020

#define ETA_M       0x001c0
#define ETA_M_SYMM  0x00040
#define ETA_M_FREE  0x00080
#define ETA_M_COMM  0x00100

#define ETA_P       0x00e00
#define ETA_P_SYMM  0x00200
#define ETA_P_FREE  0x00400
#define ETA_P_COMM  0x00800

#define ZETA_M      0x07000
#define ZETA_M_SYMM 0x01000
#define ZETA_M_FREE 0x02000
#define ZETA_M_COMM 0x04000

#define ZETA_P      0x38000
#define ZETA_P_SYMM 0x08000
#define ZETA_P_FREE 0x10000
#define ZETA_P_COMM 0x20000

// MPI Message Tags
#define MSG_COMM_SBN      1024
#define MSG_SYNC_POS_VEL  2048
#define MSG_MONOQ         3072

#define MAX_FIELDS_PER_MPI_COMM 6

// Assume 128 byte coherence
// Assume Real_t is an "integral power of 2" bytes wide
#define CACHE_COHERENCE_PAD_REAL (128 / sizeof(Real_t))

#define CACHE_ALIGN_REAL(n) \
   (((n) + (CACHE_COHERENCE_PAD_REAL - 1)) & ~(CACHE_COHERENCE_PAD_REAL-1))

//////////////////////////////////////////////////////
// Primary data structure
//////////////////////////////////////////////////////

/*
 * The implementation of the data abstraction used for lulesh
 * resides entirely in the Domain class below.  You can change
 * grouping and interleaving of fields here to maximize data layout
 * efficiency for your underlying architecture or compiler.
 *
 * For example, fields can be implemented as STL objects or
 * raw array pointers.  As another example, individual fields
 * m_x, m_y, m_z could be budled into
 *
 *    struct { Real_t x, y, z ; } *m_coord ;
 *
 * allowing accessor functions such as
 *
 *  "Real_t &x(Index_t idx) { return m_coord[idx].x ; }"
 *  "Real_t &y(Index_t idx) { return m_coord[idx].y ; }"
 *  "Real_t &z(Index_t idx) { return m_coord[idx].z ; }"
 */

#if SERDES_ENABLED
//class DomainSerdes;
#endif

class Domain {
#if SERDES_ENABLED
   friend class DomainSerdes;
#endif
   public:

   // Constructor
   Domain(Int_t numRanks, Index_t colLoc,
          Index_t rowLoc, Index_t planeLoc,
          Index_t nx, Int_t tp, Int_t nr, Int_t balance, Int_t cost);

   //
   // ALLOCATION
   //

   void AllocateNodePersistent(Int_t numNode) // Node-centered
   {
      m_x.resize(numNode);  // coordinates
      m_y.resize(numNode);
      m_z.resize(numNode);

      m_xd.resize(numNode); // velocities
      m_yd.resize(numNode);
      m_zd.resize(numNode);

      m_xdd.resize(numNode); // accelerations
      m_ydd.resize(numNode);
      m_zdd.resize(numNode);

      m_fx.resize(numNode);  // forces
      m_fy.resize(numNode);
      m_fz.resize(numNode);

      m_nodalMass.resize(numNode);  // mass
   }

   void AllocateElemPersistent(Int_t numElem) // Elem-centered
   {
      m_nodelist.resize(8*numElem);

      // elem connectivities through face
      m_lxim.resize(numElem);
      m_lxip.resize(numElem);
      m_letam.resize(numElem);
      m_letap.resize(numElem);
      m_lzetam.resize(numElem);
      m_lzetap.resize(numElem);

      m_elemBC.resize(numElem);

      m_e.resize(numElem);
      m_p.resize(numElem);

      m_q.resize(numElem);
      m_ql.resize(numElem);
      m_qq.resize(numElem);

      m_v.resize(numElem);

      m_volo.resize(numElem);
      m_delv.resize(numElem);
      m_vdov.resize(numElem);

      m_arealg.resize(numElem);

      m_ss.resize(numElem);

      m_elemMass.resize(numElem);
   }

   void AllocateGradients(Int_t numElem, Int_t allElem)
   {
      // Position gradients
      m_delx_xi.resize(numElem) ;
      m_delx_eta.resize(numElem) ;
      m_delx_zeta.resize(numElem) ;

      // Velocity gradients
      m_delv_xi.resize(allElem) ;
      m_delv_eta.resize(allElem);
      m_delv_zeta.resize(allElem) ;
   }

   void DeallocateGradients()
   {
      m_delx_zeta.clear() ;
      m_delx_eta.clear() ;
      m_delx_xi.clear() ;

      m_delv_zeta.clear() ;
      m_delv_eta.clear() ;
      m_delv_xi.clear() ;
   }

   void AllocateStrains(Int_t numElem)
   {
      m_dxx.resize(numElem) ;
      m_dyy.resize(numElem) ;
      m_dzz.resize(numElem) ;
   }

   void DeallocateStrains()
   {
      m_dzz.clear() ;
      m_dyy.clear() ;
      m_dxx.clear() ;
   }
   
   //
   // ACCESSORS
   //

   // Node-centered

   // Nodal coordinates
   Real_t& x(Index_t idx)    { return m_x[idx] ; }
   Real_t& y(Index_t idx)    { return m_y[idx] ; }
   Real_t& z(Index_t idx)    { return m_z[idx] ; }

   // Nodal velocities
   Real_t& xd(Index_t idx)   { return m_xd[idx] ; }
   Real_t& yd(Index_t idx)   { return m_yd[idx] ; }
   Real_t& zd(Index_t idx)   { return m_zd[idx] ; }

   // Nodal accelerations
   Real_t& xdd(Index_t idx)  { return m_xdd[idx] ; }
   Real_t& ydd(Index_t idx)  { return m_ydd[idx] ; }
   Real_t& zdd(Index_t idx)  { return m_zdd[idx] ; }

   // Nodal forces
   Real_t& fx(Index_t idx)   { return m_fx[idx] ; }
   Real_t& fy(Index_t idx)   { return m_fy[idx] ; }
   Real_t& fz(Index_t idx)   { return m_fz[idx] ; }

   // Nodal mass
   Real_t& nodalMass(Index_t idx) { return m_nodalMass[idx] ; }

   // Nodes on symmertry planes
   Index_t symmX(Index_t idx) { return m_symmX[idx] ; }
   Index_t symmY(Index_t idx) { return m_symmY[idx] ; }
   Index_t symmZ(Index_t idx) { return m_symmZ[idx] ; }
   bool symmXempty()          { return m_symmX.empty(); }
   bool symmYempty()          { return m_symmY.empty(); }
   bool symmZempty()          { return m_symmZ.empty(); }

   //
   // Element-centered
   //
   Index_t&  regElemSize(Index_t idx) { return m_regElemSize[idx] ; }
   Index_t&  regNumList(Index_t idx) { return m_regNumList[idx] ; }
   Index_t*  regNumList()            { return &m_regNumList[0] ; }
   Index_t*  regElemlist(Int_t r)    { return m_regElemlist[r] ; }
   Index_t&  regElemlist(Int_t r, Index_t idx) { return m_regElemlist[r][idx] ; }

   Index_t*  nodelist(Index_t idx)    { return &m_nodelist[Index_t(8)*idx] ; }

   // elem connectivities through face
   Index_t&  lxim(Index_t idx) { return m_lxim[idx] ; }
   Index_t&  lxip(Index_t idx) { return m_lxip[idx] ; }
   Index_t&  letam(Index_t idx) { return m_letam[idx] ; }
   Index_t&  letap(Index_t idx) { return m_letap[idx] ; }
   Index_t&  lzetam(Index_t idx) { return m_lzetam[idx] ; }
   Index_t&  lzetap(Index_t idx) { return m_lzetap[idx] ; }

   // elem face symm/free-surface flag
   Int_t&  elemBC(Index_t idx) { return m_elemBC[idx] ; }

   // Principal strains - temporary
   Real_t& dxx(Index_t idx)  { return m_dxx[idx] ; }
   Real_t& dyy(Index_t idx)  { return m_dyy[idx] ; }
   Real_t& dzz(Index_t idx)  { return m_dzz[idx] ; }

   // Velocity gradient - temporary
   Real_t& delv_xi(Index_t idx)    { return m_delv_xi[idx] ; }
   Real_t& delv_eta(Index_t idx)   { return m_delv_eta[idx] ; }
   Real_t& delv_zeta(Index_t idx)  { return m_delv_zeta[idx] ; }

   // Position gradient - temporary
   Real_t& delx_xi(Index_t idx)    { return m_delx_xi[idx] ; }
   Real_t& delx_eta(Index_t idx)   { return m_delx_eta[idx] ; }
   Real_t& delx_zeta(Index_t idx)  { return m_delx_zeta[idx] ; }

   // Energy
   Real_t& e(Index_t idx)          { return m_e[idx] ; }

   // Pressure
   Real_t& p(Index_t idx)          { return m_p[idx] ; }

   // Artificial viscosity
   Real_t& q(Index_t idx)          { return m_q[idx] ; }

   // Linear term for q
   Real_t& ql(Index_t idx)         { return m_ql[idx] ; }
   // Quadratic term for q
   Real_t& qq(Index_t idx)         { return m_qq[idx] ; }

   // Relative volume
   Real_t& v(Index_t idx)          { return m_v[idx] ; }
   Real_t& delv(Index_t idx)       { return m_delv[idx] ; }

   // Reference volume
   Real_t& volo(Index_t idx)       { return m_volo[idx] ; }

   // volume derivative over volume
   Real_t& vdov(Index_t idx)       { return m_vdov[idx] ; }

   // Element characteristic length
   Real_t& arealg(Index_t idx)     { return m_arealg[idx] ; }

   // Sound speed
   Real_t& ss(Index_t idx)         { return m_ss[idx] ; }

   // Element mass
   Real_t& elemMass(Index_t idx)  { return m_elemMass[idx] ; }

   Index_t nodeElemCount(Index_t idx)
   { return m_nodeElemStart[idx+1] - m_nodeElemStart[idx] ; }

   Index_t *nodeElemCornerList(Index_t idx)
   { return &m_nodeElemCornerList[m_nodeElemStart[idx]] ; }

#if 0
   enum DOMAIN_DATA_ID {
    M_X ,  /* COORDINATES */
    M_Y ,
    M_Z ,

    M_XD , /* VELOCITIES */
    M_YD ,
    M_ZD ,

    M_XDD , /* ACCELERATIONS */
    M_YDD ,
    M_ZDD ,

    M_FX ,  /* FORCES */
    M_FY ,
    M_FZ ,

    M_NODALMASS ,  /* MASS */

    M_SYMMX ,  /* SYMMETRY PLANE NODESETS */
    M_SYMMY ,
    M_SYMMZ ,


    M_MATELEMLIST ,  /* MATERIAL INDEXSET */
    M_NODELIST ,     /* ELEMTONODE CONNECTIVITY */

    M_LXIM ,  /* ELEMENT CONNECTIVITY ACROSS EACH FACE */
    M_LXIP ,
    M_LETAM ,
    M_LETAP ,
    M_LZETAM ,
    M_LZETAP ,

    M_ELEMBC ,  /* SYMMETRY/FREE-SURFACE FLAGS FOR EACH ELEM FACE */

    M_DXX ,  /* PRINCIPAL STRAINS -- TEMPORARY */
    M_DYY ,
    M_DZZ ,

    M_DELV_XI ,    /* VELOCITY GRADIENT -- TEMPORARY */
    M_DELV_ETA ,
    M_DELV_ZETA ,

    M_DELX_XI ,    /* COORDINATE GRADIENT -- TEMPORARY */
    M_DELX_ETA ,
    M_DELX_ZETA ,
   
    M_E_ ,   /* ENERGY */

    M_P ,   /* PRESSURE */
    M_Q ,   /* Q */
    M_QL ,  /* LINEAR TERM FOR Q */
    M_QQ ,  /* QUADRATIC TERM FOR Q */

    M_V ,     /* RELATIVE VOLUME */
    M_VOLO ,  /* REFERENCE VOLUME */
    M_VNEW ,  /* NEW RELATIVE VOLUME -- TEMPORARY */
    M_DELV ,  /* M_VNEW - M_V */
    M_VDOV ,  /* VOLUME DERIVATIVE OVER VOLUME */

    M_AREALG ,  /* CHARACTERISTIC LENGTH OF AN ELEMENT */
   
    M_SS ,      /* "SOUND SPEED" */

    M_ELEMMASS ,  /* MASS */

    TOTAL_ELEMENT_COUNT

   };


   void *Serialize(uint32_t &len_in_bytes) {
      Packer packer;
      packer.Add(M_X, sizeof(Real_t) * m_x.size(), m_x.data());
      packer.Add(M_Y, sizeof(Real_t) * m_y.size(), m_y.data());
      packer.Add(M_Z, sizeof(Real_t) * m_z.size(), m_z.data());

      packer.Add(M_XD, sizeof(Real_t) * m_xd.size(), m_xd.data());
      packer.Add(M_YD, sizeof(Real_t) * m_yd.size(), m_yd.data());
      packer.Add(M_ZD, sizeof(Real_t) * m_zd.size(), m_zd.data());

      packer.Add(M_XDD, sizeof(Real_t) * m_xdd.size(), m_xdd.data());
      packer.Add(M_YDD, sizeof(Real_t) * m_ydd.size(), m_ydd.data());
      packer.Add(M_ZDD, sizeof(Real_t) * m_zdd.size(), m_zdd.data());

      packer.Add(M_FX, sizeof(Real_t) * m_fx.size(), m_x.data());
      packer.Add(M_FY, sizeof(Real_t) * m_fy.size(), m_y.data());
      packer.Add(M_FZ, sizeof(Real_t) * m_fz.size(), m_z.data());

      packer.Add(M_NODALMASS, sizeof(Real_t) * m_nodalMass.size(), m_nodalMass.data());

      packer.Add(M_SYMMX, sizeof(Index_t) * m_symmX.size(), m_symmX.data());
      packer.Add(M_SYMMY, sizeof(Index_t) * m_symmY.size(), m_symmY.data());
      packer.Add(M_SYMMZ, sizeof(Index_t) * m_symmZ.size(), m_symmZ.data());

      packer.Add(M_MATELEMLIST, sizeof(Index_t) * m_matElemlist.size(), m_matElemlist.data());
      packer.Add(M_NODELIST, sizeof(Index_t) * m_nodelist.size(), m_nodelist.data());


      packer.Add(M_LXIM,   sizeof(Index_t) * m_lxim.size(),   m_lxim.data());  
      packer.Add(M_LXIP,   sizeof(Index_t) * m_lxip.size(),   m_lxip.data());  
      packer.Add(M_LETAM,  sizeof(Index_t) * m_letam.size(),  m_letam.data()); 
      packer.Add(M_LETAP,  sizeof(Index_t) * m_letap.size(),  m_letap.data()); 
      packer.Add(M_LZETAM, sizeof(Index_t) * m_lzetam.size(), m_lzetam.data());
      packer.Add(M_LZETAP, sizeof(Index_t) * m_lzetap.size(), m_lzetap.data());


      packer.Add(M_ELEMBC, sizeof(Index_t) * m_elemBC.size(), m_elemBC.data());
  
      packer.Add(M_DXX, sizeof(Real_t) * m_dxx.size(),  m_dxx.data());
      packer.Add(M_DYY, sizeof(Real_t) * m_dyy.size(),  m_dyy.data());
      packer.Add(M_DZZ, sizeof(Real_t) * m_dzz.size(),  m_dzz.data());

      packer.Add(M_DELV_XI,   sizeof(Real_t) * m_delv_xi.size(),   m_delv_xi.data());   
      packer.Add(M_DELV_ETA,  sizeof(Real_t) * m_delv_eta.size(),  m_delv_eta.data());  
      packer.Add(M_DELV_ZETA, sizeof(Real_t) * m_delv_zeta.size(), m_delv_zeta.data()); 
                                                                        
      packer.Add(M_DELX_XI,   sizeof(Real_t) * m_delx_xi.size(),   m_delx_xi.data());   
      packer.Add(M_DELX_ETA,  sizeof(Real_t) * m_delx_eta.size(),  m_delx_eta.data());  
      packer.Add(M_DELX_ZETA, sizeof(Real_t) * m_delx_zeta.size(), m_delx_zeta.data()); 

      packer.Add(M_E_,  sizeof(Real_t) * m_e.size(),  m_e.data()); 
                                                   
      packer.Add(M_P,  sizeof(Real_t) * m_p.size(),  m_p.data()); 
      packer.Add(M_Q,  sizeof(Real_t) * m_q.size(),  m_q.data()); 
      packer.Add(M_QL, sizeof(Real_t) * m_ql.size(), m_ql.data());
      packer.Add(M_QQ, sizeof(Real_t) * m_qq.size(), m_qq.data());

      packer.Add(M_V,    sizeof(Real_t) * m_v.size(),    m_v.data());   
      packer.Add(M_VOLO, sizeof(Real_t) * m_volo.size(), m_volo.data());
      packer.Add(M_VNEW, sizeof(Real_t) * m_vnew.size(), m_vnew.data());
      packer.Add(M_DELV, sizeof(Real_t) * m_delv.size(), m_delv.data());
      packer.Add(M_VDOV, sizeof(Real_t) * m_vdov.size(), m_vdov.data());

      packer.Add(M_AREALG,   sizeof(Real_t) * m_arealg.size(),   m_arealg.data()); 
                                                                      
      packer.Add(M_SS,       sizeof(Real_t) * m_ss.size(),       m_ss.data());         
                                                                      
      packer.Add(M_ELEMMASS, sizeof(Real_t) * m_elemMass.size(), m_elemMass.data());

      return packer.GetTotalData(len_in_bytes);
   }
#endif

   // Parameters 

   // Cutoffs
   Real_t u_cut() const               { return m_u_cut ; }
   Real_t e_cut() const               { return m_e_cut ; }
   Real_t p_cut() const               { return m_p_cut ; }
   Real_t q_cut() const               { return m_q_cut ; }
   Real_t v_cut() const               { return m_v_cut ; }

   // Other constants (usually are settable via input file in real codes)
   Real_t hgcoef() const              { return m_hgcoef ; }
   Real_t qstop() const               { return m_qstop ; }
   Real_t monoq_max_slope() const     { return m_monoq_max_slope ; }
   Real_t monoq_limiter_mult() const  { return m_monoq_limiter_mult ; }
   Real_t ss4o3() const               { return m_ss4o3 ; }
   Real_t qlc_monoq() const           { return m_qlc_monoq ; }
   Real_t qqc_monoq() const           { return m_qqc_monoq ; }
   Real_t qqc() const                 { return m_qqc ; }

   Real_t eosvmax() const             { return m_eosvmax ; }
   Real_t eosvmin() const             { return m_eosvmin ; }
   Real_t pmin() const                { return m_pmin ; }
   Real_t emin() const                { return m_emin ; }
   Real_t dvovmax() const             { return m_dvovmax ; }
   Real_t refdens() const             { return m_refdens ; }

   // Timestep controls, etc...
   Real_t& time()                 { return m_time ; }
   Real_t& deltatime()            { return m_deltatime ; }
   Real_t& deltatimemultlb()      { return m_deltatimemultlb ; }
   Real_t& deltatimemultub()      { return m_deltatimemultub ; }
   Real_t& stoptime()             { return m_stoptime ; }
   Real_t& dtcourant()            { return m_dtcourant ; }
   Real_t& dthydro()              { return m_dthydro ; }
   Real_t& dtmax()                { return m_dtmax ; }
   Real_t& dtfixed()              { return m_dtfixed ; }

   Int_t&  cycle()                { return m_cycle ; }
   Index_t&  numRanks()           { return m_numRanks ; }

   Index_t&  colLoc()             { return m_colLoc ; }
   Index_t&  rowLoc()             { return m_rowLoc ; }
   Index_t&  planeLoc()           { return m_planeLoc ; }
   Index_t&  tp()                 { return m_tp ; }

   Index_t&  sizeX()              { return m_sizeX ; }
   Index_t&  sizeY()              { return m_sizeY ; }
   Index_t&  sizeZ()              { return m_sizeZ ; }
   Index_t&  numReg()             { return m_numReg ; }
   Int_t&  cost()             { return m_cost ; }
   Index_t&  numElem()            { return m_numElem ; }
   Index_t&  numNode()            { return m_numNode ; }
   
   Index_t&  maxPlaneSize()       { return m_maxPlaneSize ; }
   Index_t&  maxEdgeSize()        { return m_maxEdgeSize ; }
   
   //
   // MPI-Related additional data
   //

#if USE_MPI   
   // Communication Work space 
   Real_t *commDataSend ;
   Real_t *commDataRecv ;
   
   // Maximum number of block neighbors 
   MPI_Request recvRequest[26] ; // 6 faces + 12 edges + 8 corners 
   MPI_Request sendRequest[26] ; // 6 faces + 12 edges + 8 corners 
#endif

  private:

   void BuildMesh(Int_t nx, Int_t edgeNodes, Int_t edgeElems);
   void SetupThreadSupportStructures();
   void CreateRegionIndexSets(Int_t nreg, Int_t balance);
   void SetupCommBuffers(Int_t edgeNodes);
   void SetupSymmetryPlanes(Int_t edgeNodes);
   void SetupElementConnectivities(Int_t edgeElems);
   void SetupBoundaryConditions(Int_t edgeElems);

   //
   // IMPLEMENTATION
   //
#if SERDES_ENABLED
class DomainSerdes : public Serializable {
   int method;
   const Domain *dom;
   enum DOMAIN_DATA_ID {
    M_X ,  /* COORDINATES */
    M_Y ,
    M_Z ,

    M_XD , /* VELOCITIES */
    M_YD ,
    M_ZD ,

    M_XDD , /* ACCELERATIONS */
    M_YDD ,
    M_ZDD ,

    M_FX ,  /* FORCES */
    M_FY ,
    M_FZ ,

    M_NODALMASS ,  /* MASS */

    M_SYMMX ,  /* SYMMETRY PLANE NODESETS */
    M_SYMMY ,
    M_SYMMZ ,


    M_MATELEMLIST ,  /* MATERIAL INDEXSET */
    M_NODELIST ,     /* ELEMTONODE CONNECTIVITY */

    M_LXIM ,  /* ELEMENT CONNECTIVITY ACROSS EACH FACE */
    M_LXIP ,
    M_LETAM ,
    M_LETAP ,
    M_LZETAM ,
    M_LZETAP ,

    M_ELEMBC ,  /* SYMMETRY/FREE-SURFACE FLAGS FOR EACH ELEM FACE */

    M_DXX ,  /* PRINCIPAL STRAINS -- TEMPORARY */
    M_DYY ,
    M_DZZ ,

    M_DELV_XI ,    /* VELOCITY GRADIENT -- TEMPORARY */
    M_DELV_ETA ,
    M_DELV_ZETA ,

    M_DELX_XI ,    /* COORDINATE GRADIENT -- TEMPORARY */
    M_DELX_ETA ,
    M_DELX_ZETA ,
   
    M_E_ ,   /* ENERGY */

    M_P ,   /* PRESSURE */
    M_Q ,   /* Q */
    M_QL ,  /* LINEAR TERM FOR Q */
    M_QQ ,  /* QUADRATIC TERM FOR Q */

    M_V ,     /* RELATIVE VOLUME */
    M_VOLO ,  /* REFERENCE VOLUME */
    M_VNEW ,  /* NEW RELATIVE VOLUME -- TEMPORARY */
    M_DELV ,  /* M_VNEW - M_V */
    M_VDOV ,  /* VOLUME DERIVATIVE OVER VOLUME */

    M_AREALG ,  /* CHARACTERISTIC LENGTH OF AN ELEMENT */
   
    M_SS ,      /* "SOUND SPEED" */

    M_ELEMMASS ,  /* MASS */

    SERDES_ALL,

    TOTAL_ELEMENT_COUNT

   };
  public:
    DomainSerdes(void) {
      method = 3;
      dom = NULL;
    }
    DomainSerdes(const Domain *domain) {
      method = 3;
      dom = domain;
    }
    void SetSerdesOption (int option) {
      method = option;
    }
    
    // With this interface, user can switch to a different serializer with a method flag.
    virtual void *Serialize(uint32_t &len_in_bytes) {
      // User define whatever they want.
      cout << "[UserClass2] ";
      switch(method) {
        case SERDES_ALL:
          return SerializeAll(len_in_bytes);
        case 1:
          return SerializeMethod1(len_in_bytes);
        case 2:
          return SerializeMethod2(len_in_bytes);
        case 3:
          return SerializeMethod3(len_in_bytes);
        defualt:
          return NULL;
      }
    }

    virtual void Deserialize(void *object) {
    // Deserialize object and restore to each member.
      cout << "[UserClass2] ";
    
      switch(method) {
        case 0:
          DeserializeAll(object);
          break;
        case 1:
          DeserializeMethod1(object);
          break;
        case 2:
          DeserializeMethod2(object);
          break;
        case 3:
        DeserializeMethod3(object);
          break;
        defualt:
          return;
      }
    }

    void *SerializeAll(uint32_t &len_in_bytes) {
      // Serialize Method 0
      cout << "Serialize Method 0\n" << endl;
      Packer packer;
      packer.Add(M_X, sizeof(Real_t)   * dom->m_x.size(), dom->m_x.data());
      packer.Add(M_Y, sizeof(Real_t)   * dom->m_y.size(), dom->m_y.data());
      packer.Add(M_Z, sizeof(Real_t)   * dom->m_z.size(), dom->m_z.data());

      packer.Add(M_XD, sizeof(Real_t)  * dom->m_xd.size(), dom->m_xd.data());
      packer.Add(M_YD, sizeof(Real_t)  * dom->m_yd.size(), dom->m_yd.data());
      packer.Add(M_ZD, sizeof(Real_t)  * dom->m_zd.size(), dom->m_zd.data());

      packer.Add(M_XDD, sizeof(Real_t) * dom->m_xdd.size(), dom->m_xdd.data());
      packer.Add(M_YDD, sizeof(Real_t) * dom->m_ydd.size(), dom->m_ydd.data());
      packer.Add(M_ZDD, sizeof(Real_t) * dom->m_zdd.size(), dom->m_zdd.data());

      packer.Add(M_FX, sizeof(Real_t)  * dom->m_fx.size(), dom->m_x.data());
      packer.Add(M_FY, sizeof(Real_t)  * dom->m_fy.size(), dom->m_y.data());
      packer.Add(M_FZ, sizeof(Real_t)  * dom->m_fz.size(), dom->m_z.data());

      packer.Add(M_NODALMASS, sizeof(Real_t) * dom->m_nodalMass.size(), dom->m_nodalMass.data());

      packer.Add(M_SYMMX, sizeof(Index_t) * dom->m_symmX.size(), dom->m_symmX.data());
      packer.Add(M_SYMMY, sizeof(Index_t) * dom->m_symmY.size(), dom->m_symmY.data());
      packer.Add(M_SYMMZ, sizeof(Index_t) * dom->m_symmZ.size(), dom->m_symmZ.data());

      packer.Add(M_MATELEMLIST, sizeof(Index_t) * dom->m_matElemlist.size(), dom->m_matElemlist.data());
      packer.Add(M_NODELIST, sizeof(Index_t) * dom->m_nodelist.size(), dom->m_nodelist.data());

      packer.Add(M_LXIM,   sizeof(Index_t) * dom->m_lxim.size(),   dom->m_lxim.data());  
      packer.Add(M_LXIP,   sizeof(Index_t) * dom->m_lxip.size(),   dom->m_lxip.data());  
      packer.Add(M_LETAM,  sizeof(Index_t) * dom->m_letam.size(),  dom->m_letam.data()); 
      packer.Add(M_LETAP,  sizeof(Index_t) * dom->m_letap.size(),  dom->m_letap.data()); 
      packer.Add(M_LZETAM, sizeof(Index_t) * dom->m_lzetam.size(), dom->m_lzetam.data());
      packer.Add(M_LZETAP, sizeof(Index_t) * dom->m_lzetap.size(), dom->m_lzetap.data());


      packer.Add(M_ELEMBC, sizeof(Index_t) * dom->m_elemBC.size(), dom->m_elemBC.data());
  
      packer.Add(M_DXX, sizeof(Real_t) * dom->m_dxx.size(),  dom->m_dxx.data());
      packer.Add(M_DYY, sizeof(Real_t) * dom->m_dyy.size(),  dom->m_dyy.data());
      packer.Add(M_DZZ, sizeof(Real_t) * dom->m_dzz.size(),  dom->m_dzz.data());

      packer.Add(M_DELV_XI,   sizeof(Real_t) * dom->m_delv_xi.size(),   dom->m_delv_xi.data());   
      packer.Add(M_DELV_ETA,  sizeof(Real_t) * dom->m_delv_eta.size(),  dom->m_delv_eta.data());  
      packer.Add(M_DELV_ZETA, sizeof(Real_t) * dom->m_delv_zeta.size(), dom->m_delv_zeta.data()); 
                                                                        
      packer.Add(M_DELX_XI,   sizeof(Real_t) * dom->m_delx_xi.size(),   dom->m_delx_xi.data());   
      packer.Add(M_DELX_ETA,  sizeof(Real_t) * dom->m_delx_eta.size(),  dom->m_delx_eta.data());  
      packer.Add(M_DELX_ZETA, sizeof(Real_t) * dom->m_delx_zeta.size(), dom->m_delx_zeta.data()); 

      packer.Add(M_E_,  sizeof(Real_t) * dom->m_e.size(),  dom->m_e.data()); 
                                                   
      packer.Add(M_P,  sizeof(Real_t) * dom->m_p.size(),  dom->m_p.data()); 
      packer.Add(M_Q,  sizeof(Real_t) * dom->m_q.size(),  dom->m_q.data()); 
      packer.Add(M_QL, sizeof(Real_t) * dom->m_ql.size(), dom->m_ql.data());
      packer.Add(M_QQ, sizeof(Real_t) * dom->m_qq.size(), dom->m_qq.data());

      packer.Add(M_V,    sizeof(Real_t) * dom->m_v.size(),    dom->m_v.data());   
      packer.Add(M_VOLO, sizeof(Real_t) * dom->m_volo.size(), dom->m_volo.data());
      packer.Add(M_VNEW, sizeof(Real_t) * dom->m_vnew.size(), dom->m_vnew.data());
      packer.Add(M_DELV, sizeof(Real_t) * dom->m_delv.size(), dom->m_delv.data());
      packer.Add(M_VDOV, sizeof(Real_t) * dom->m_vdov.size(), dom->m_vdov.data());

      packer.Add(M_AREALG,   sizeof(Real_t) * dom->m_arealg.size(),   dom->m_arealg.data()); 
                                                                      
      packer.Add(M_SS,       sizeof(Real_t) * dom->m_ss.size(),       dom->m_ss.data());         
                                                                      
      packer.Add(M_ELEMMASS, sizeof(Real_t) * dom->m_elemMass.size(), dom->m_elemMass.data());

      return packer.GetTotalData(len_in_bytes);
    }
    void *SerializeMethod1(uint32_t &len_in_bytes) {
      // Serialize Method 1
      cout << "Serialize Method 1\n" << endl;
    }
    void *SerializeMethod2(uint32_t &len_in_bytes) {
      // Serialize Method 2
      cout << "Serialize Method 2\n" << endl;
    }
    void *SerializeMethod3(uint32_t &len_in_bytes) {
      // Serialize Method 3
      cout << "Serialize Method 3\n" << endl;
    }
    
    void DeserializeAll(void *object) {
      // Deserialize Method 0
      cout << "Deserialize Method 0\n" << endl;
    }
    void DeserializeMethod1(void *object) {
      // Deserialize Method 1
      cout << "Deserialize Method 1\n" << endl;
    }
    void DeserializeMethod2(void *object) {
      // Deserialize Method 2
      cout << "Deserialize Method 2\n" << endl;
    }
    void DeserializeMethod3(void *object) {
      // Deserialize Method 3
      cout << "Deserialize Method 3\n" << endl;
    }
};
#endif
#if SERDES_ENABLED
   // CD
   DomainSerdes serdes;
#endif
   /* Node-centered */
   std::vector<Real_t> m_x ;  /* coordinates */
   std::vector<Real_t> m_y ;
   std::vector<Real_t> m_z ;

   std::vector<Real_t> m_xd ; /* velocities */
   std::vector<Real_t> m_yd ;
   std::vector<Real_t> m_zd ;

   std::vector<Real_t> m_xdd ; /* accelerations */
   std::vector<Real_t> m_ydd ;
   std::vector<Real_t> m_zdd ;

   std::vector<Real_t> m_fx ;  /* forces */
   std::vector<Real_t> m_fy ;
   std::vector<Real_t> m_fz ;

   std::vector<Real_t> m_nodalMass ;  /* mass */

   std::vector<Index_t> m_symmX ;  /* symmetry plane nodesets */
   std::vector<Index_t> m_symmY ;
   std::vector<Index_t> m_symmZ ;

   // Element-centered

   // Region information
   Int_t    m_numReg ;
   Int_t    m_cost; //imbalance cost
   Index_t *m_regElemSize ;   // Size of region sets
   Index_t *m_regNumList ;    // Region number per domain element
   Index_t **m_regElemlist ;  // region indexset 

   std::vector<Index_t>  m_matElemlist ;  /* material indexset */
   std::vector<Index_t>  m_nodelist ;     /* elemToNode connectivity */

   std::vector<Index_t>  m_lxim ;  /* element connectivity across each face */
   std::vector<Index_t>  m_lxip ;
   std::vector<Index_t>  m_letam ;
   std::vector<Index_t>  m_letap ;
   std::vector<Index_t>  m_lzetam ;
   std::vector<Index_t>  m_lzetap ;

   std::vector<Int_t>    m_elemBC ;  /* symmetry/free-surface flags for each elem face */

   std::vector<Real_t> m_dxx ;  /* principal strains -- temporary */
   std::vector<Real_t> m_dyy ;
   std::vector<Real_t> m_dzz ;

   std::vector<Real_t> m_delv_xi ;    /* velocity gradient -- temporary */
   std::vector<Real_t> m_delv_eta ;
   std::vector<Real_t> m_delv_zeta ;

   std::vector<Real_t> m_delx_xi ;    /* coordinate gradient -- temporary */
   std::vector<Real_t> m_delx_eta ;
   std::vector<Real_t> m_delx_zeta ;
   
   std::vector<Real_t> m_e ;   /* energy */

   std::vector<Real_t> m_p ;   /* pressure */
   std::vector<Real_t> m_q ;   /* q */
   std::vector<Real_t> m_ql ;  /* linear term for q */
   std::vector<Real_t> m_qq ;  /* quadratic term for q */

   std::vector<Real_t> m_v ;     /* relative volume */
   std::vector<Real_t> m_volo ;  /* reference volume */
   std::vector<Real_t> m_vnew ;  /* new relative volume -- temporary */
   std::vector<Real_t> m_delv ;  /* m_vnew - m_v */
   std::vector<Real_t> m_vdov ;  /* volume derivative over volume */

   std::vector<Real_t> m_arealg ;  /* characteristic length of an element */
   
   std::vector<Real_t> m_ss ;      /* "sound speed" */

   std::vector<Real_t> m_elemMass ;  /* mass */

   // Cutoffs (treat as constants)
   const Real_t  m_e_cut ;             // energy tolerance 
   const Real_t  m_p_cut ;             // pressure tolerance 
   const Real_t  m_q_cut ;             // q tolerance 
   const Real_t  m_v_cut ;             // relative volume tolerance 
   const Real_t  m_u_cut ;             // velocity tolerance 

   // Other constants (usually setable, but hardcoded in this proxy app)

   const Real_t  m_hgcoef ;            // hourglass control 
   const Real_t  m_ss4o3 ;
   const Real_t  m_qstop ;             // excessive q indicator 
   const Real_t  m_monoq_max_slope ;
   const Real_t  m_monoq_limiter_mult ;
   const Real_t  m_qlc_monoq ;         // linear term coef for q 
   const Real_t  m_qqc_monoq ;         // quadratic term coef for q 
   const Real_t  m_qqc ;
   const Real_t  m_eosvmax ;
   const Real_t  m_eosvmin ;
   const Real_t  m_pmin ;              // pressure floor 
   const Real_t  m_emin ;              // energy floor 
   const Real_t  m_dvovmax ;           // maximum allowable volume change 
   const Real_t  m_refdens ;           // reference density 

   // Variables to keep track of timestep, simulation time, and cycle
   Real_t  m_dtcourant ;         // courant constraint 
   Real_t  m_dthydro ;           // volume change constraint 
   Int_t   m_cycle ;             // iteration count for simulation 
   Real_t  m_dtfixed ;           // fixed time increment 
   Real_t  m_time ;              // current time 
   Real_t  m_deltatime ;         // variable time increment 
   Real_t  m_deltatimemultlb ;
   Real_t  m_deltatimemultub ;
   Real_t  m_dtmax ;             // maximum allowable time increment 
   Real_t  m_stoptime ;          // end time for simulation 


   Int_t   m_numRanks ;

   Index_t m_colLoc ;
   Index_t m_rowLoc ;
   Index_t m_planeLoc ;
   Index_t m_tp ;

   Index_t m_sizeX ;
   Index_t m_sizeY ;
   Index_t m_sizeZ ;
   Index_t m_numElem ;
   Index_t m_numNode ;

   Index_t m_maxPlaneSize ;
   Index_t m_maxEdgeSize ;

   // OMP hack 
   Index_t *m_nodeElemStart ;
   Index_t *m_nodeElemCornerList ;

   // Used in setup
   Index_t m_rowMin, m_rowMax;
   Index_t m_colMin, m_colMax;
   Index_t m_planeMin, m_planeMax ;

} ;

typedef Real_t &(Domain::* Domain_member )(Index_t) ;

struct cmdLineOpts {
   Int_t its; // -i 
   Int_t nx;  // -s 
   Int_t numReg; // -r 
   Int_t numFiles; // -f
   Int_t showProg; // -p
   Int_t quiet; // -q
   Int_t viz; // -v 
   Int_t cost; // -c
   Int_t balance; // -b
};



// Function Prototypes

// lulesh-par
Real_t CalcElemVolume( const Real_t x[8],
                       const Real_t y[8],
                       const Real_t z[8]);

// lulesh-util
void ParseCommandLineOptions(int argc, char *argv[],
                             Int_t myRank, struct cmdLineOpts *opts);
void VerifyAndWriteFinalOutput(Real_t elapsed_time,
                               Domain& locDom,
                               Int_t nx,
                               Int_t numRanks);

// lulesh-viz
void DumpToVisit(Domain& domain, int numFiles, int myRank, int numRanks);

// lulesh-comm
void CommRecv(Domain& domain, Int_t msgType, Index_t xferFields,
              Index_t dx, Index_t dy, Index_t dz,
              bool doRecv, bool planeOnly);
void CommSend(Domain& domain, Int_t msgType,
              Index_t xferFields, Domain_member *fieldData,
              Index_t dx, Index_t dy, Index_t dz,
              bool doSend, bool planeOnly);
void CommSBN(Domain& domain, Int_t xferFields, Domain_member *fieldData);
void CommSyncPosVel(Domain& domain);
void CommMonoQ(Domain& domain);

// lulesh-init
void InitMeshDecomp(Int_t numRanks, Int_t myRank,
                    Int_t *col, Int_t *row, Int_t *plane, Int_t *side);



