        SUBROUTINE atimesb(A,B,C,n)
                
!***********************************************************************
!        Written by Kumar Mithraratne
!        � Auckland Bioengineering Institute
!        V1.1 July 2013
!***********************************************************************
        
        IMPLICIT NONE
        
        INCLUDE 'rigidbody1.cmn'
                
        REAL*8 A(3,3),B(3,3),C(3,3)
        INTEGER n
        
        !REAL*8 
        !INTEGER 
        
        C(1,1)=A(1,1)*B(1,1)+A(1,2)*B(2,1)+A(1,3)*B(3,1)
        C(1,2)=A(1,1)*B(1,2)+A(1,2)*B(2,2)+A(1,3)*B(3,2)
        C(1,3)=A(1,1)*B(1,3)+A(1,2)*B(2,3)+A(1,3)*B(3,3)
        C(2,1)=A(2,1)*B(1,1)+A(2,2)*B(2,1)+A(2,3)*B(3,1)
        C(2,2)=A(2,1)*B(1,2)+A(2,2)*B(2,2)+A(2,3)*B(3,2)
        C(2,3)=A(2,1)*B(1,3)+A(2,2)*B(2,3)+A(2,3)*B(3,3)
        C(3,1)=A(3,1)*B(1,1)+A(3,2)*B(2,1)+A(3,3)*B(3,1)
        C(3,2)=A(3,1)*B(1,2)+A(3,2)*B(2,2)+A(3,3)*B(3,2)
        C(3,3)=A(3,1)*B(1,3)+A(3,2)*B(2,3)+A(3,3)*B(3,3)


        RETURN
        END

