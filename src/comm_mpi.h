#ifdef MODULES_OPTION_CHECK
   if( (strcmp(argv[i+1], "MPI") == 0) )
     comm_type = COMM_MPI;
#endif
#ifdef MODULES_INSTANTIATION
   else if(input->comm_type == COMM_MPI) {
     comm = new CommMPI(system,input->force_cutoff + input->neighbor_skin);
   }
#endif


#if !defined(MODULES_OPTION_CHECK) && !defined(MODULES_INSTANTIATION)
#ifndef COMM_MPI_H
#define COMM_MPI_H
#include<comm.h>

#ifndef EXAMINIMD_ENABLE_MPI
#error "Trying to compile CommMPI without MPI"
#endif

#include "mpi.h"

class CommMPI: public Comm {

  // Variables Comm doesn't own but requires for computations

  T_INT N_local;
  T_INT N_ghost;

  System s;

  // Owned Variables

  int phase; // Communication Phase
  int proc_neighbors[6]; // Neighbor for each phase
  int proc_num_recv[6];  // Number of received particles in each phase
  int proc_num_send[6];  // Number of send particles in each phase
  int proc_pos[3];       // My process position
  int proc_grid[3];      // Process Grid size
  int proc_rank;         // My Process rank
  int proc_size;         // Number of processes

  T_INT num_packed;
  Kokkos::View<int, Kokkos::MemoryTraits<Kokkos::Atomic> > pack_count;
  Kokkos::View<Particle*> pack_buffer;
  Kokkos::View<Particle*> unpack_buffer;

  Kokkos::View<T_INT**,Kokkos::LayoutRight> pack_indicies_all;
  Kokkos::View<T_INT*,Kokkos::LayoutRight,Kokkos::MemoryTraits<Kokkos::Unmanaged> > pack_indicies;
  Kokkos::View<T_INT*,Kokkos::LayoutRight > exchange_dest_list;

public:

  struct TagUnpack {};

  struct TagExchangeSelf {};
  struct TagExchangePack {};
  struct TagExchangeCreateDestList {};
  struct TagExchangeCompact {};
  
  struct TagHaloSelf {};
  struct TagHaloPack {};
  struct TagHaloUpdateSelf {};
  struct TagHaloUpdatePack {};
  struct TagHaloUpdateUnpack {};

  struct TagPermuteIndexList {};

  CommMPI(System* s, T_X_FLOAT comm_depth_);
  void init();
  void create_domain_decomposition();
  void permute_index_lists(Binning::t_permute_vector& permute_vector);
  void exchange();
  void exchange_halo();
  void update_halo();
  void reduce_int(T_INT* vals, T_INT count);
  void reduce_float(T_FLOAT* vals, T_INT count);

  KOKKOS_INLINE_FUNCTION
  void operator() (const TagExchangeSelf, 
                   const T_INT& i) const {
    if(proc_grid[0]==1) {
      const T_X_FLOAT x = s.x(i,0);
      if(x>s.domain_x) s.x(i,0) -= s.domain_x;
      if(x<0)          s.x(i,0) += s.domain_x;
    }
    if(proc_grid[1]==1) {
      const T_X_FLOAT y = s.x(i,1);
      if(y>s.domain_y) s.x(i,1) -= s.domain_y;
      if(y<0)          s.x(i,1) += s.domain_y;
    }
    if(proc_grid[2]==1) {
      const T_X_FLOAT z = s.x(i,2);
      if(z>s.domain_z) s.x(i,2) -= s.domain_z;
      if(z<0)          s.x(i,2) += s.domain_z;
    }   
  }

  KOKKOS_INLINE_FUNCTION
  void operator() (const TagExchangePack, 
                   const T_INT& i) const {
    if(s.type(i)<0) return;
    if( (phase == 0) && (s.x(i,0)>s.sub_domain_hi_x)) {
      const int pack_idx = pack_count()++;
      if( pack_idx < pack_indicies.extent(0) ) {
        pack_indicies(pack_idx) = i; 
        Particle p = s.get_particle(i);
        s.type(i) = -1;
        if(proc_pos[0] == proc_grid[0]-1)
          p.x -= s.domain_x;
        pack_buffer(pack_idx) = p;
      }
    }  

    if( (phase == 1) && (s.x(i,0)<s.sub_domain_lo_x)) {
      const int pack_idx = pack_count()++;
      if( pack_idx < pack_indicies.extent(0) ) {
        pack_indicies(pack_idx) = i;
        Particle p = s.get_particle(i);
        s.type(i) = -1;
        if(proc_pos[0] == 0)
          p.x += s.domain_x;
        pack_buffer(pack_idx) = p;
      }
    }

    if( (phase == 2) && (s.x(i,1)>s.sub_domain_hi_y)) {
      const int pack_idx = pack_count()++;
      if( pack_idx < pack_indicies.extent(0) ) {
        pack_indicies(pack_idx) = i;
        Particle p = s.get_particle(i);
        s.type(i) = -1;
        if(proc_pos[1] == proc_grid[1]-1)
          p.y -= s.domain_y;
        pack_buffer(pack_idx) = p;
      }
    }
    if( (phase == 3) && (s.x(i,1)<s.sub_domain_lo_y)) {
      const int pack_idx = pack_count()++;
      if( pack_idx < pack_indicies.extent(0) ) {
        pack_indicies(pack_idx) = i;
        Particle p = s.get_particle(i);
        s.type(i) = -1;
        if(proc_pos[1] == 0)
          p.y += s.domain_y;
        pack_buffer(pack_idx) = p;
      }
    }

    if( (phase == 4) && (s.x(i,2)>s.sub_domain_hi_z)) {
      const int pack_idx = pack_count()++;
      if( pack_idx < pack_indicies.extent(0) ) {
        pack_indicies(pack_idx) = i;
        Particle p = s.get_particle(i);
        s.type(i) = -1;
        if(proc_pos[2] == proc_grid[2]-1)
          p.z -= s.domain_z;
        pack_buffer(pack_idx) = p;
      }
    }
    if( (phase == 5) && (s.x(i,2)<s.sub_domain_lo_z)) {
      const int pack_idx = pack_count()++;
      if( pack_idx < pack_indicies.extent(0) ) {
        pack_indicies(pack_idx) = i;
        Particle p = s.get_particle(i);
        s.type(i) = -1;
        if(proc_pos[2] == 0)
          p.z += s.domain_z;
        pack_buffer(pack_idx) = p;
      }
    }
  } 

  KOKKOS_INLINE_FUNCTION
  void operator() (const TagExchangeCreateDestList,
                   const T_INT& i, T_INT& c, const bool final) const {
    if(s.type(i)<0) {
      if(final) {
        exchange_dest_list(c) = i;
      }
      c++;
    }
  }

  KOKKOS_INLINE_FUNCTION
  void operator() (const TagExchangeCompact,
                   const T_INT& ii, T_INT& c, const bool final) const {
    const T_INT i = N_local+N_ghost-1-ii;
    if(s.type(i)>=0) {
      if(final) {
        s.copy(exchange_dest_list(c),i,0,0,0);
      }
      c++;
    }
  }

  
  KOKKOS_INLINE_FUNCTION
  void operator() (const TagHaloSelf,
                   const T_INT& i) const {
    if(phase == 0) {
      if( s.x(i,0)>=s.sub_domain_hi_x - comm_depth ) {
        const int pack_idx = pack_count()++;
        if((pack_idx < pack_indicies.extent(0)) && (N_local+N_ghost+pack_idx< s.x.extent(0))) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          p.x -= s.domain_x;
          s.set_particle(N_local + N_ghost + pack_idx, p);
        }
      }
    }
    if(phase == 1) {
      if( s.x(i,0)<=s.sub_domain_lo_x + comm_depth ) {
        const int pack_idx = pack_count()++;
        if((pack_idx < pack_indicies.extent(0)) && (N_local+N_ghost+pack_idx< s.x.extent(0))) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          p.x += s.domain_x;
          s.set_particle(N_local + N_ghost + pack_idx, p);
        }
      }
    }
    if(phase == 2) {
      if( s.x(i,1)>=s.sub_domain_hi_y - comm_depth ) {
        const int pack_idx = pack_count()++;
        if((pack_idx < pack_indicies.extent(0)) && (N_local+N_ghost+pack_idx< s.x.extent(0))) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          p.y -= s.domain_y;
          s.set_particle(N_local + N_ghost + pack_idx, p);
        }
      }
    }
    if(phase == 3) {
      if( s.x(i,1)<=s.sub_domain_lo_y + comm_depth ) {
        const int pack_idx = pack_count()++;
        if((pack_idx < pack_indicies.extent(0)) && (N_local+N_ghost+pack_idx< s.x.extent(0))) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          p.y += s.domain_y;
          s.set_particle(N_local + N_ghost + pack_idx, p);
        }
      }
    }
    if(phase == 4) {
      if( s.x(i,2)>=s.sub_domain_hi_z - comm_depth ) {
        const int pack_idx = pack_count()++;
        if((pack_idx < pack_indicies.extent(0)) && (N_local+N_ghost+pack_idx< s.x.extent(0))) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          p.z -= s.domain_z;
          s.set_particle(N_local + N_ghost + pack_idx, p);
        }
      }
    }
    if(phase == 5) {
      if( s.x(i,2)<=s.sub_domain_lo_z + comm_depth ) {
        const int pack_idx = pack_count()++;
        if((pack_idx < pack_indicies.extent(0)) && (N_local+N_ghost+pack_idx< s.x.extent(0))) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          p.z += s.domain_z;
          s.set_particle(N_local + N_ghost + pack_idx, p);
        }
      }
    }

  }

  KOKKOS_INLINE_FUNCTION
  void operator() (const TagHaloPack,
                   const T_INT& i) const {
    if(phase == 0) {
      if( s.x(i,0)>=s.sub_domain_hi_x - comm_depth ) {
        const int pack_idx = pack_count()++;
        if(pack_idx < pack_indicies.extent(0)) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          if(proc_pos[0] == proc_grid[0]-1)
            p.x -= s.domain_x;
          pack_buffer(pack_idx) = p;
        }
      }
    }
    if(phase == 1) {
      if( s.x(i,0)<=s.sub_domain_lo_x + comm_depth ) {
        const int pack_idx = pack_count()++;
        if(pack_idx < pack_indicies.extent(0)) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          if(proc_pos[0] == 0)
            p.x += s.domain_x;
          pack_buffer(pack_idx) = p;
        }
      }
    }
    if(phase == 2) {
      if( s.x(i,1)>=s.sub_domain_hi_y - comm_depth ) {
        const int pack_idx = pack_count()++;
        if(pack_idx < pack_indicies.extent(0)) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          if(proc_pos[1] == proc_grid[1]-1)
            p.y -= s.domain_y;
          pack_buffer(pack_idx) = p;
        }
      }
    }
    if(phase == 3) {
      if( s.x(i,1)<=s.sub_domain_lo_y + comm_depth ) {
        const int pack_idx = pack_count()++;
        if(pack_idx < pack_indicies.extent(0)) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          if(proc_pos[1] == 0)
            p.y += s.domain_y;
          pack_buffer(pack_idx) = p;
        }
      }
    }
    if(phase == 4) {
      if( s.x(i,2)>=s.sub_domain_hi_z - comm_depth ) {
        const int pack_idx = pack_count()++;
        if(pack_idx < pack_indicies.extent(0)) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          if(proc_pos[2] == proc_grid[2]-1)
            p.z -= s.domain_z;
          pack_buffer(pack_idx) = p;
        }
      }
    }
    if(phase == 5) {
      if( s.x(i,2)<=s.sub_domain_lo_z + comm_depth ) {
        const int pack_idx = pack_count()++;
        if(pack_idx < pack_indicies.extent(0)) {
          pack_indicies(pack_idx) = i;
          Particle p = s.get_particle(i);
          if(proc_pos[2] == 0)
            p.z += s.domain_z;
          pack_buffer(pack_idx) = p;
        }
      }
    }
  }

  KOKKOS_INLINE_FUNCTION
  void operator() (const TagUnpack,
                   const T_INT& i) const {
    s.set_particle(N_local+N_ghost+i, unpack_buffer(i));
  }


  KOKKOS_INLINE_FUNCTION
  void operator() (const TagHaloUpdateSelf,
                   const T_INT& i) const {

    Particle p = s.get_particle(pack_indicies(i));
    switch (phase) {
      case 0: p.x -= s.domain_x; break;
      case 1: p.x += s.domain_x; break;
      case 2: p.y -= s.domain_y; break;
      case 3: p.y += s.domain_y; break;
      case 4: p.z -= s.domain_z; break;
      case 5: p.z += s.domain_z; break;
    }
    s.set_particle(N_local + N_ghost + i, p);     
  }

  KOKKOS_INLINE_FUNCTION
  void operator() (const TagHaloUpdatePack,
                   const T_INT& i) const {

    Particle p = s.get_particle(pack_indicies(i));
    switch (phase) {
      case 0: if(proc_pos[0] == proc_grid[0]-1) p.x -= s.domain_x; break;
      case 1: if(proc_pos[0] == 0)              p.x += s.domain_x; break;
      case 2: if(proc_pos[1] == proc_grid[1]-1) p.y -= s.domain_y; break;
      case 3: if(proc_pos[1] == 0)              p.y += s.domain_y; break;
      case 4: if(proc_pos[2] == proc_grid[2]-1) p.z -= s.domain_z; break;
      case 5: if(proc_pos[2] == 0)              p.z += s.domain_z; break;
    }
    pack_buffer(i) = p;
  }

  const char* name();
  int process_rank();
};

#endif
#endif // MODULES_OPTION_CHECK
