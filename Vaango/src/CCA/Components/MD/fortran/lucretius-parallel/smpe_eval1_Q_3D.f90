   subroutine smpe_eval1_Q_3D
! Eval stresses and energy for Q_POINT charges only
     use variables_smpe
     use Ewald_data
     use sim_cel_data
     use boundaries, only : cel_properties, get_reciprocal_cut
     use sizes_data, only : Natoms
     use energies_data
     use cut_off_data
     use stresses_data, only : stress, stress_Qcmplx

     implicit none
     real(8), parameter :: four_pi_sq = 39.4784176043574d0
     real(8), parameter :: Pi2 = 6.28318530717959d0
     integer i,j,k,ix,iy,iz,jx,jy,jz,nx,ny,nz,nx0,ny0,nz0
     integer m_iy1,m_iy2,m_ix1,m_ix2,m_iz1,m_iz2,m_ix,m_iy,m_iz
     integer ii_xx,ii_yy,ii_zz
     integer jy1,jx1,jz1,i_index
     real(8) sxx,sxy,sxz,syy,syz,szz
     real(8) expfct,i4a2,i4a2_2, tmp, vterm,vterm2
     real(8) spline_product, En, Eni, ff, vir,vir0
     real(8) temp_vir_cz, t
     real(8) qim, qre, exp_fct
     real(8) rec_xx,rec_yy,rec_zz,rec_xy,rec_yx,rec_xz,rec_zx,rec_zy,rec_yz
     real(8) d2,i_d2
     real(8) kx , ky, kz
     real(8) local_potential
     real(8) area
     real(8) local_potential1, En1, local_potential_PP,local_potential_GG,local_potential_PG
     real(8) lsxx,lsyy,lszz,lsxy,lsxz,lsyx,lsyz,lszx,lszy


! array qqq1 contains data for energy
! array qqq2 contains data for stresses xz,yz,zz


     call get_reciprocal_cut

     nx0 = nfftx/2 ; ny0 = nffty/2 ; nz0 = nfftz/2
     i4a2 = 0.25d0/(Ewald_alpha*Ewald_alpha)
     expfct = - i4a2
     i4a2_2 = 2.0d0 * i4a2


     local_potential = 0.0d0
     lsxx = 0.0d0; lsxy = 0.0d0; lsxz = 0.0d0
     lsyx = 0.0d0; lsyy = 0.0d0; lsyz = 0.0d0
     lszx = 0.0d0; lszy = 0.0d0; lszz = 0.0d0

     do jz = 1,nfftz
       jz1 = jz-1
       if (jz > nz0) jz1 = jz1 - nfftz
       tmp = dble(jz1)
       rec_xz = tmp*Reciprocal_cel(3)
       rec_yz = tmp*Reciprocal_cel(6)
       rec_zz = tmp*Reciprocal_cel(9)
       do jy = 1,nffty
          jy1 = jy - 1
          if (jy > ny0) jy1 = jy1 - nffty
          tmp = dble(jy1)
          rec_xy = tmp*Reciprocal_cel(2)
          rec_yy = tmp*Reciprocal_cel(5)
          rec_zy = tmp*Reciprocal_cel(8)
!          rec_yy = dble(jy1) * Reciprocal_Sim_Box(2)
          do jx = 1, nfftx
            jx1 = jx -1
            if (jx > nx0) jx1 = jx1 - nfftx
            tmp = dble(jx1)
            rec_xx = tmp*Reciprocal_cel(1)
            rec_yx = tmp*Reciprocal_cel(4)
            rec_zx = tmp*Reciprocal_cel(7)
            i_index = ((jy-1)+(jz-1)*nffty)*nfftx + jx
            kz = rec_zx + rec_zy + rec_zz
            kx = rec_xx + rec_xy + rec_xz
            ky = rec_yx + rec_yy + rec_yz
            d2 = kx*kx + ky*ky + kz*kz
 if (d2 < reciprocal_cut_sq.and.jz1**2+jy1**2+jx1**2 /= 0) then
              i_d2 = 1.0d0/d2
              exp_fct = dexp(expfct*d2) * i_d2
!              qre = real(qqq1(i_index),kind=8) ;    qim=dimag(qqq1(i_index))
              spline_product = spline2_CMPLX_xx(jx)*spline2_CMPLX_yy(jy)*spline2_CMPLX_zz(jz)
              vterm =  exp_fct / (Volume*spline_product) * Pi2
              En = vterm*real(qqq1(i_index)*conjg(qqq1(i_index)),kind=8)  !*(qre*qre+qim*qim)
             local_potential = local_potential + En
             vir0 = 2.0d0*(i_d2 + i4a2)
             vir = vir0 * En
             lsxx = lsxx + En - vir*kx*kx ;
             lsxy = lsxy - vir*kx*ky  ;
             lsyy = lsyy + En - vir*ky*ky ;
             lsxz = lsxz - vir*kx*kz
             lsyz = lsyz - vir*ky*kz
             lszz = lszz + En - vir*kz*kz

!print*, jx,jy,jz,'term=',vir*kx*kx, vir*ky*ky, vir*kz*kz, vir*kx*ky,vir*kx*kz,vir*ky*kz
!print*,kx,vir0
!read(*,*)

             qqq1(i_index) = qqq1(i_index)*vterm  

 else
              qqq1(i_index) = 0.0d0
 endif     !  reciprocal_cutt within cut off        
        enddo
     enddo
    enddo


   En_Q_cmplx = En_Q_cmplx + local_potential
   En_Q = En_Q + local_potential
   stress_Qcmplx(1) =  lsxx
   stress_Qcmplx(2) =  lsyy
   stress_Qcmplx(3) =  lszz
   stress_Qcmplx(4) = (lsxx+lsyy+lszz)/3.0d0
   stress_Qcmplx(5) =  lsxy
   stress_Qcmplx(6) =  lsxz
   stress_Qcmplx(7) =  lsyz
   stress_Qcmplx(8) =  lsyx  ! sxy = syx if no dipols!!!!
   stress_Qcmplx(9) =  lszx  ! change it when dipols are in
   stress_Qcmplx(10) = lszy  ! change it when dipols are in

   stress(:) = stress(:) + stress_Qcmplx(:)


 end subroutine smpe_eval1_Q_3D ! energy and stresses


