#
# The MIT License
#
# Copyright (c) 2013-2014 Callaghan Innovation, New Zealand
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

import numpy as np
    
def uSG( x, h ):
    r = abs(x)
    sgnx = cmp(x,-x)
    
    if ( r < h ):
        S = 1. - r/h
        G = -sgnx/h
    else: 
        S = G = 0.
    return( S,G )
	

def getCell( patch, pos ):    
    # Gets lower left node of 4-cell block
    x_sc = (pos - patch.X0)/patch.dX + patch.nGhost
    idx = np.floor(x_sc)
    ii = idx[0] 
    jj = idx[1] 
	
    return int(jj * patch.Nc[0] + ii)
    

def updateContribList( dw, patch, dwi ):
    # Update node contribution list
    nx = patch.Nc[0]
    h = patch.dX
    idxs = [0,1,nx,nx+1]
    S = np.zeros(h.size)
    G = np.zeros(h.size)	
    
    cIdx,cW,cGrad = dw.getMult( ['cIdx','cW','cGrad'], dwi )
    px,gx = dw.getMult( ['px','gx'], dwi )

    for ii in range(len(pVol)):
        cc = getCell( patch, px[ii] )	           

        for jj in range(4):	
            idx = idxs[jj] + cc 
            r = px[ii] - gx[idx]	
		
            for kk in range(len(r)):
                S[kk],G[kk] = uSG( r[kk], h[kk] )
		
            cIdx[ii][jj] = idx
            cW[ii][jj] = S[0]*S[1]
            cGrad[ii][jj] = G * S[::-1] 