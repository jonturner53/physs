/** Lib object provides generic methods for analyzing spectra */
class Library {

    constructor () {
        this.waveguideLength = 0.28;   // set when deployment record is read
        this.nlcCoef = [1];            // coefficients for nonlinear correction

        this.rawWavelengths = [];      // ditto
        this.rawLo = 0;                // index of last wavelength < MINWAVE
        this.rawHi = 0;                // index of first wavelength > MAXWAVE

        this.MINWAVE = 350;            // min cooked wavelength
        this.MAXWAVE = 800;            // max+ cooked wavelength 
        this.CDOM_MINWAVE = 390;       // min wavelength for cdom param
		this.CDOM_MAXWAVE = 490;       // max+ wavelength for cdom param
        this.SIM_MINWAVE = 400;        // min wavelength for sim index
		this.SIM_MAXWAVE = 700;        // max+ wavelength for sim index

		this.cookedWavelengths = [];
		this.setCookedWavelengths(1);
    
        this.smoothSettings = [23, 12];		// default smoothing parameters
        this.derivSettings = [7, 28];		// default deriv parameters
    }

	waveIndex(w) { return Math.round((w - this.MINWAVE) * this.waveDensity) }

    setWavelengths(wavelengths) {
        this.rawWavelengths = wavelengths;
        this.rawLo = wavelengths.findIndex(w => w >= lib.MINWAVE) - 1;
        this.rawHi = wavelengths.findIndex(w => w > lib.MAXWAVE);
    }

	setCookedWavelengths(waveDensity) {
        this.waveDensity = waveDensity; // # cooked wavelengths per nm (1 or 2)

        this.cookedWavelengths =
            this.vectorAdd(this.vectorScale(
                this.range(0, this.waveIndex(this.MAXWAVE)),
                1/this.waveDensity), this.MINWAVE);
    
		// index range for CDOM wavelengths
        this.CDOM_LO = this.waveIndex(this.CDOM_MINWAVE);
        this.CDOM_HI = this.waveIndex(this.CDOM_MAXWAVE);
    
		// index range for sim index wavelengths
        this.SIM_LO = this.waveIndex(this.SIM_MINWAVE);
        this.SIM_HI = this.waveIndex(this.SIM_MAXWAVE);
	}

    setWaveguideLength(length) {
        this.waveguideLength = length;
    }

    setNlcCoef(coef) { this.nlcCoef = coef; }

    nonlinearCorrection(v, coef=lib.nlcCoef) {
        let cv = new Array(v.length);
        for (let i = 0; i < v.length; i++) {
            let s = 0;
            for (let j = coef.length-1; j >= 0; j--)
                s += coef[j] + v[i] * s;
            cv[i] = v[i] / s;
        }
        return cv;
    }

    /** Compute an integer range.
     *  @param lo is a non-negative integer
     *  @param hi is an integer >lo
     *  @return the range [0,...,hi-1]
     */
    range(lo, hi) {
       let r = []; r.length = hi - lo;
       let v = lo;
       for (let i = 0; i < r.length; i++) r[i] = v++;
       return r;
    }

    /** Apply a gaussian filter to a vector.
     *  @param ivec is a vector of floats (assumed to be longer than 2w+1)
     *  @param width specifies the width of the filter; specifically,
     *  the filter has 2*width+1 terms
     *  @param sigma specifies the "spread" of the filter
     *  @return a vector of floats with same length as ivec

Possible generalization. 
smooth(x, y, settings) where x,y define a function.
That is, we don't assume fixed spacing. Optionally, we could
allow a scalar x value to denote a fixed spacing.
In general case, use the x vector when smooothing.
Does mean that we lose efficiency advantage of a common filter.
Not clear this matters in the grand scheme of things. In a file
with 2000 spectra to smooth, we add 4M*50 invocations of Math.exp.
At 1 us per invocation, that's 20 extra seconds to preprocess data.
Note, we also do this repeatedly in time series, but could avoid
that by saving cooked spectrum and using it when available.

     */
    smooth(ivec, settings=lib.smoothSettings) {
        let [width, sigma] = settings;
        // compute filter terms
        let fsum = 0.0;
        let filter = new Array(2*width+1);
        for (let i = 0; i < 2*width+1; i++) {
            let x = 0.0 + i - width;
            filter[i] = Math.exp(-(x*x)/(2.*sigma*sigma));
            fsum += filter[i];
        }
        for (let i = 0; i < 2*width+1; i++) filter[i] /= fsum;
    
        // apply filter to middle of vector
        let n = ivec.length;
        let ovec = new Array(n).fill(0);
        for (let i = width; i < n-width; i++) {
            for (let j = 0; j < 2*width+1; j++)
                ovec[i] += filter[j] * ivec[(i-width)+j];
        }
    
        // apply filter to ends of vector;
        for (let i = 1; i < width; i++) {
            let t = 0.0;
            for (let j = 0; j < 2*i+1; j++) {
                ovec[i] += filter[j+width-i] * ivec[j];
                ovec[(n-1)-i] += filter[j+width-i] * ivec[(n-1)-j];
                t += filter[j+width-i];
            }
            ovec[i] /= t; ovec[(n-1)-i] /= t;
        }
        ovec[n-1] = ivec[n-1];
        return ovec;
    };

    /** Standardize a function to use specified x values.
     *  Uses simple interpolation from neighboring pairs of points.
     *  @param x is an increasing vector x coordinates
     *  @param y is a vector y coordinates; together x and y define a function
	 *  @param rx is an array of x values to be used with the result vector
     *  @return a vector of y-values for an approximation of the original
     *  function that uses the x values in rx
     */
    standardize(x, y, rx) {
        let result = new Array(rx.length);
        let j = 0;
        for (let i = 0; i < rx.length; i++) {
            // find first value in x greater than lo+i
            while (x[j] <= rx[i]) j++;
            // interpolate to get value at i
            let f = (rx[i] - x[j-1]) / (x[j] - x[j-1]);
            result[i] = (1.0-f)*y[j-1] + f*y[j];
        }
        return result;
    };

    /** Fit a line to a set of points, using least-squares.
     *  @param x is a vector of x values
     *  @param y is a vector of y values with same length as x
     *  @return the y-intercept and slope of the least-squares line
     *  fitting the points 
     */
    fitline(x,y) {
        let n = x.length;  // also assumed to be y.length
        let xbar = lib.vectorAvg(x);
        let ybar = lib.vectorAvg(y);
        let dx = 0.; let dy = 0.; let i;
        for (i = 0; i < n; i++) {
            dx += (x[i]-xbar)*(x[i]-xbar); dy += (x[i]-xbar)*(y[i]-ybar);
        }
        let slope = dy/dx;
        let y0 = ybar - slope*xbar;
        return [y0, slope];
    }
    
    /** Shift a vector vertically.
     *  @param vec is a vector to be shifted
     *  @param width is width of window used to determine the shift amount;
     *  specifically, the average value of vec is computed for each window
     *  of the specified width; the smallest of the window averages is
     *  subtracted from all values of vec to produce the shifted vector
     *  @return the shifted version of vec
     */
    vectorShift(vec, width=50) {
        let windowSum = 0;
        for (let i = 0; i < width; i++) windowSum += vec[i];
        let minSum = windowSum;
        for (let i = width; i < vec.length; i++) {
            windowSum = windowSum + vec[i] - vec[i-width];
            if (windowSum < minSum) minSum = windowSum;
        }
        let shift = minSum/width;
    
        let result = [];
        for (let i = 0; i < vec.length; i++) {
            result.push(Math.max(0, vec[i] - shift));
        }
        return result;
    }

    /** Compute derivative of a function.
     *  @param f is a vector representing value of a function at equally
     *  spaced points
     *  @param k is the index of derivative required (1 for first derivative,
     *  2 for second derivative, etc)
     *  @param settings is a pair of parameters [deg, w] that specify a
     *  polynomial approximation used to compute the derivative at each point
	 *  @param delta is the interval between x values
     *  @return a vector representing the derivative values at the same points
     *  as f
     */
    deriv(f, k, settings=lib.derivSettings, delta=1) {
        let [deg, w] = settings;
        if (k < 0 || k > deg-1) return null;

        // set up array of monomials to fit to each window of f
        let M = new Array(deg+1);
        for (let i = 0; i <= deg; i++) {
            M[i] = new Array(2*w+1);
            for (let j = 0; j <= 2*w; j++)
                M[i][j] = (i == 0 ? 1 : (j-w)*delta*M[i-1][j]);
        }
        let A = new Array(deg+1);
        for (let i = 0; i <= deg; i++) {
            A[i] = new Array(deg+1);
            for (let j = 0; j <= deg; j++)
                A[i][j] = (i <= j ? lib.dotProduct(M[i], M[j]) : A[j][i]);
        }
        // Now AxW = MxV is matrix equation for finding best weights
        // W to match a vector V; rearrange to give W=(A^{-1}xM)xV=ZxV
        let Ai = lib.matrixInvert(A);
        let z = lib.matrixProduct(Ai, M);

        // now Z can be used to compute weights for each window of f
        let C = 1;
        for (let i = 2; i <= k; i++) C *= i;
        let n = f.length;
        let deriv = new Array(n);
        deriv.fill(0, 0, w); deriv.fill(0, n-w);
        for (let i = w; i < n-w; i++) {
            deriv[i] = C * lib.dotProduct(z[k], f.slice(i-w, i+w+1));
        }
        return deriv;
    }

    /** Find the value of a function at a specified x-coordinate.
     *  @param xv is a sorted vector of x coordinates
     *  @param yv is a matching vector of y coordinates
     *  @param x is a specified y coordinate
     *  @return the value in yv for the largest coordinate in xv that is no
     *  larger than x; if no such x coordinate, return the last value in yv
     */
    functionValue(xv, yv, x) {
        let n = Math.min(xv.length, yv.length);
        if (n == 0) return 0;
        if (n == 1) return yv[0];
        if (xv[0] >= x) return yv[0];
        if (xv[n-1] <= x) return yv[n-1];
        let lo = 0; let hi = n-1;
        while (lo <= hi) {
            let mid = Math.floor((hi+lo)/2);
            if (xv[mid] <= x && xv[mid+1] > x) return yv[mid];
            else if (xv[mid] < x) lo = mid+1;
            else hi = mid-1;
        }
    }

    /* In a non-decreasing vector, identify values in specified range.
     * @param vec is a vector sorted in non-decreasing order
     * @param lo is a minimum data value
     * @param hi is a maximum data value
     * @return a pair of integers i and j for which vec[i]..vec[j-1] are all in
     * the half-open interval [lo,hi); more specifically, i and j define the
     * the largest such range of values
     */
    findSpan(vec, lo, hi) {
        let i, j;
        for (i = 0; i < vec.length; i++)
            if (vec[i] >= lo) break;
        if (i == vec.length) return [0,0];
        for (j = i; j < vec.length; j++)
            if (vec[j] >= hi) break;
        return [i,j];
    }
    
    /** Find the smallest value in a vector.
     *  @param v is a numeric vector
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     */
    vectorMin(v, rake=[0,v.length]) {
        let m = v[rake[0]];
        for (let k = 0; k < rake.length; k += 2) {
            for (let i = rake[k]; i < rake[k+1]; i++)
                m = Math.min(m, v[i]);
        }
        return m;
    }
    
    /** Find the largest value in a vector.
     *  @param v is a numeric vector
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     */
    vectorMax(v, rake=[0,v.length]) {
        let m = v[rake[0]];
        for (let k = 0; k < rake.length; k+= 2) {
            for (let i = rake[k]; i < rake[k+1]; i++)
                m = Math.max(m, v[i]);
        }
        return m;
    }
    
    /** Compute the average of the values in a vector.
     *  @param x is a numeric vector
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     */
    vectorAvg(x, rake=[0,x.length]) {
        let s = 0; let n = 0;
        for (let k = 0; k < rake.length; k+= 2) {
            for (let i = rake[k]; i < rake[k+1]; i++) {
                s += x[i]; n++;
            }
        }
        return s / n;
    }

    /** Add a scalar to a vector, or add two vectors.
     *  If v2 a number it is added to v1
     *  and the resulting vector is returned. If v2 has more than one element,
     *  return the sum of the two vectors.
     */
    vectorAdd(v1, v2) {
        let n = v1.length;
        let v = new Array(n);
        if (Array.isArray(v2)) {
            for (let i = 0; i < n; i++) v[i] = v1[i] + v2[i];
        } else {
            for (let i = 0; i < n; i++) v[i] = v1[i] + v2;
        }
        return v;
    }
    
    /** Subtract a scalar from a vector, or take the difference of
     *  two vectors.
     *  If v2 is a scalar, it is subtracted from all elements of v1
     *  and the resulting vector is returned. If v2 has more than one element,
     *  return the vector formed from the pairwise differences.
     */
    vectorSubtract(v1, v2) {
        let n = v1.length
        let v = new Array(n);
        if (Array.isArray(v2)) {
            for (let i = 0; i < n; i++) v[i] = v1[i] - v2[i];
        } else {
            for (let i = 0; i < n; i++) v[i] = v1[i] - v2;
        }
        return v;
    }

    /** Compute a smoothed version of a vector.
     *  @param vec is a vector
     *  @param width is the width of an averaging window (default 1)
     *  @return a result vector, where result is the average
     *  of elements vec[i-width+1],...,vec[i]
     */
    vectorSmooth(vec, width=1) {
        if (width <= 0) width = 1;
        if (width == 1) return vec.slice(0);
        let result = new Array(vec.length);
        let s = 0;
        for (let i = 0; i < vec.length; i++) {
            s += vec[i];
            if (i >= width) s -= vec[i-width];
            result[i] = s / Math.min(i+1, width);
        }
        return result;
    }

    /** Scale a vector by a constant.
     *  @param vec is a vector
     *  @param c is a constant
     *  @return the vector c*vec
     */
    vectorScale(vec, c) {
        let result = new Array(vec.length); 
        for (let i = 0; i < vec.length; i++) result[i] = c * vec[i];
        return result;
    }

    /** Determine if two vectors are equal to each other.
     *  @param x is a vector
     *  @param y is a vector of the same length
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return true if x, y have equal values
     */
    vectorEqual(x, y, rake=[0,x.length]) {
        for (let k = 0; k < rake.length; k+= 2) {
            for (let i = rake[k]; i < rake[k+1]; i++)
                if (x[i] !== y[i]) return false;
        }
        return true;
    }

    /** Vector dot-product (aka inner product).
     *  @param a is a vector
     *  @param b is a second vector of the same length
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return the sum of the pairwise products of a and b.
     */
    dotProduct(a, b, rake=[0,a.length]) {
        let sum = 0;
        for (let k = 0; k < rake.length; k += 2) {
            for (let i = rake[k]; i < rake[k+1]; i++)
                sum += a[i] * b[i];
        }
        return sum;
    }

    /** Compute the product of two matrices.
     *  @param A is a 2d matrix (an array of equal length vectors);
     *  note: there is no special case format for row/column vectors
     *  @param B is a matrix with a column length equal to the row
     *  length of A
     *  @return the matrix product of A and B
     */
    matrixProduct(A, B) {
        let n = A.length; let m = B[0].length;
        if (B.length != A[0].length) return null;
        let Bt = lib.matrixTranspose(B);
        let P = new Array(n);
        for (let i = 0; i < n; i++) {
            P[i] = new Array(m);
            for (let j = 0; j < m; j++)
                P[i][j] = lib.dotProduct(A[i], Bt[j]);
        }
        return P;
    }

    /** Compute the magnitude of a vector.
     *  @param v is a vector
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return the magnitude of v
     */
    vectorMagnitude(v, rake=[0,v.length]) {
        return Math.sqrt(lib.dotProduct(v, v, rake));
    }

    /** Compute the component of one vector in the direction of another.
     *  @param a is a numeric vector
     *  @param b is a vector of the same length
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return the length of the projection of vector a on b.
     */
    vectorComponent(a, b, rake=[0,a.length]) {
        return lib.dotProduct(a, b, rake) / lib.vectorMagnitude(b, rake);
    }

    /** Compute the projection of one vector in the direction of another.
     *  @param a is a numeric vector
     *  @param b is a vector of the same length
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return the length of the projection of vector a onto b.
     */
    vectorProjection(a, b, rake=[0,a.length]) {
        return lib.vectorScale(b, lib.dotProduct(a, b, rake) /
                                   (lib.vectorMagnitude(b, rake) ** 2));
    }

    /** Compute the cosine similarity of two vectors.
     *  @param a is a vector
     *  @param b is a vector of the same length
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return the cosine of the angle between a and b
     */
    cosineSim(a, b, rake=[0,a.length]) {
        let x = lib.vectorMagnitude(a, rake) * lib.vectorMagnitude(b, rake);
        return Math.min(1, (x == 0 ? 0 : lib.dotProduct(a, b, rake) / x));
    }

    /** Compute the angular index of two vectors.
     *  @param a is a vector
     *  @param b is a vector of the same length
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return the angular similarity of a and b
     */
    angularSim(a, b, rake=[0,a.length]) {
        return 1 - (2 / Math.PI) * Math.acos(lib.cosineSim(a, b, rake));
    }

    /** Find a polynomial to fit a given set of points.
     *  @param x is a vector of x values
     *  @param y is a matching vector of y values
     *  @param k is the order of the polynomial to fit to the
     *  given x,y points
     *  @param return a vector of coefficients for the
     *  computed polynomial
     */
    polyfit(x, y, k) {
        let xs = new Array(2*k + 1); xs.fill(0);
        let ys = new Array(k + 1); ys.fill(0);
        // for j in [0,2k], compute xs[j] = sum_i x[i]**j
        // and ys[j] = sum_i y[i] * x[i]**j
        for (let i = 0; i < x.length; i++) {
            let p = 1;
            for (let j = 0; j < xs.length; j++) {
                xs[j] += p;
                if (j <= k) ys[j] += y[i] * p;
                p *= x[i];
            }
        }
        let A = []; A.length = k+1;
        for (let i = 0; i <= k; i++) {
            A[i] = []; A[i].length = k+1;
            for (let j = 0; j <= k; j++) A[i][j] = xs[i+j];
        }
        return lib.leqSolve(A, ys);
    }

    /** Find weighted sum of vectors to fit a target vector.
     *  @param v is a vector to be fit
     *  @param A is an array of vectors, with each vector the same length as v
     *  @param ss is a vector that specifies a subset of A's row indexes;
     *  it is assumed to be terminated by a value of -1
     *  @param helper is an object containing precomputed data used to speed
     *  up the computation; it contains the fields
     *  - P is s 2d array of dot products of all rows of A
     *  - Q is a vector of dot products of v with all rows of A
     *  - vdotv is the dot product of v with itself
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return a pair consisting of a vector of weights and a squared error
     *  value; the vector of weights, when applied to the rows in A
     *  gives a weighted sum that minimizes the squared error with v;
     *  when s is defined, the weighted sum is over the specified subset of A
     */
    vectorFit(v, A, ss=null, helper=null, rake=[0,v.length]) {
        if (ss == null) {
            ss = lib.range(0, A.length+1); ss[ss.length-1] = -1;
        }
        let n = ss.indexOf(-1);
        if (n < 0) return null;
        if (helper == null) {
            // define helper using just specified subset of A
            let AA = new Array(n);
            for (let i = 0; i < n; i++) AA[i] = A[ss[i]];
            helper = lib.vectorFitHelper(v, AA, null, rake);
            ss = lib.range(0, AA.length+1); ss[ss.length-1] = -1;
            n = ss.length-1;
        }
        // construct arrays P and Q using specified subsets of helper.[PQ]
        let P = new Array(n); let Q = new Array(n);
        for (let i = 0; i < n; i++) {
            P[i] = new Array(n);
            for (let j = 0; j < n; j++) P[i][j] = helper.P[ss[i]][ss[j]];
            Q[i] = helper.Q[ss[i]];
        }
        let W = lib.leqSolve(P, Q);
        if (W == null) return [ [], 0 ];

        let sqErr = helper.vdotv;
        for (let i = 0; i < n; i++) {
            sqErr -= 2 * W[i] * Q[i];
            for (let j = 0; j < n; j++) {
                sqErr += W[i] * W[j] * P[i][j];
            }
        }
        return [ W, sqErr ];
    }

    /** Create a helper object for the vectorFit method.
     *  @param v is a vector to be fit
     *  @param A is an array of vectors, with each vector the same length as v
     *  @param P is optional NxN array where N is the number of rows in A
     *  @return an object containing the following fields
     *  - P is a 2d array of dot products of all rows of A
     *  - Q is a vector of dot products of v with all rows of A
     *  - vdotv is the dot product of v with itself
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     */
    vectorFitHelper(v, A, P=null, rake=[0,v.length]) {
        let n = A.length;
        if (P === null) {    
            P = new Array(n);
            for (let i = 0; i < n; i++) {
                P[i] = new Array(n);
                for (let j = 0; j < n; j++)
                    P[i][j] = (i > j ? P[j][i] :
                                          lib.dotProduct(A[i], A[j], rake));
            }
        }
        let Q = new Array(n);
        for (let i = 0; i < n; i++) {
            Q[i] = lib.dotProduct(v, A[i], rake);
        }
        let vdotv = lib.dotProduct(v, v, rake);
        return { P: P, Q: Q, vdotv: vdotv }
    }

    /** Calculate the number of valid array indices defined by a rake.
     *  @param rake is an increasing vector of integers that are used to
     *  select valid subranges from an array; for even values of i, rake[i]
     *  is the first index in a valid sub-range; for odd values of i,
     *  rake[i] is the first index in an invalid sub-range; the length
     *  of the rake must be even.
     *  @return the total number of array indices that are considered valid
     */
    rakeSize(rake) {
        let n = 0;
        for (let k = 0; k < rake.length; k += 2)
            n += rake[k+1] - rake[k];
        return n;
    }

    /** Apply a rake to a vector.
     *  @param v is a vector
     *  @param rake is an increasing vector of integers that are used to
     *  select valid subranges from v
     *    @return a vector containing the valid subranges of v
     */
    applyRake(v, rake) {
        let result = new Array(lib.rakeSize(rake));
        let i = 0;
        for (let k = 0; k < rake.length; k += 2) {
            for (let j = rake[k]; j < rake[k+1]; j++)
                result[i++] = v[j];
        }
        return result;
    }

    /** Crop a vector using a rake.
     *  @param v is a vector
     *  @param rake defines valid subranges of v
     *  @return a version of v that is 0 in all invalid subranges
     */
    vectorCrop(v, rake) {
        let vv = v.slice();
        vv.fill(0, 0, rake[0]); vv.fill(0, rake[rake.length-1]);
        for (let k = 2; k < rake.length; k += 2)
            vv.fill(0, rake[k-1], rake[k]);
        return vv;
    }

    /** Join pair of rakes
     *  @param a is a rake used to select valid subranges from a vector
     *  @param b is another rake
     *  @return the rake that defines subranges that are valid in both a and b
     */
    joinRakes(a, b) {
        let r = []; let j = 0; let k = 0;
        for (let i = 0; i < a.length; i += 2) {
            // a[i]..a[i+1] is a valid range
            while (b[j] <= a[i]) j++;
            if (j&1) { r.push(a[i]); k++ }
            while (b[j] < a[i+1]) {
                if (j&1) { // start of invalid b range
                    if (k&1) { r.push(b[j]); k++; }
                } else { // start of a valid b range
                    if (!(k&1)) { r.push(b[j]); k++; }
                }
                j++;
            }
            if (j&1) { r.push(a[i+1]); k++ }
        }
        return r;
    }

    joinRakeSet(s) {
        if (s.length == 0) return null;
        let r = s[0].slice();
        for (let i = 1; i < s.length; i++)
            r = lib.joinRakes(r, s[i]);
        return r;
    }

    /** Create a rake from a vector.
     *  @param x is a vector
     *  @param valid is a function with arguments x and i, where x==v[i],
     *  and which returns true if x is to be considered a valid element of v
     *  @param return the rake that defines the valid elements of v
     */
    makeRake(x, valid) {
        let rake = []; let v = false;
        for (let i = 0; i < x.length; i++) {
            if (v == valid(x[i], i)) continue;
            rake.push(i); v = !v;
        }
        if (v) rake.push(x.length);
        return rake;
    }

    /** Solve a set of linear equations.
     *  @param A is an n*n matrix of coefficients.
     *  @param B is a vector with n values.
     *  @return the vector X that solves the equation A*X=B
     *  or the empty vector if no unique solution
     */
    leqSolve(A, B) {
        let n = A.length;
        if (n < 1 || n != A[0].length) return [];

        let Ai = lib.matrixInvert(A);
        if (Ai == null) return null;

        let R = new Array(n);
        for (let i = 0; i < n; i++) R[i] = lib.dotProduct(Ai[i], B);
        return R;
    }

    /** Invert matrix using gauss-jordan elimination.
     *  @param AA is a square matrix
     *  @return the inverse of AA or null if not invertible
     */
    matrixInvert(AA) {
        let n = AA.length;
        if (n < 1 || n != AA[0].length) return null;

        // make local copy of AA plus identity matrix B
        let A = new Array(n); let B = new Array(n);
        for (let i = 0; i < n; i++) {
            A[i] = AA[i].slice(0);
            B[i] = new Array(n);
            B[i].fill(0); B[i][i] = 1;
        }

        // gauss-jordan elimination
        for (let h = 0; h < n; h++) {
            let iMax = h; let maxVal = Math.abs(A[h][h]);
            for (let i = h+1; i < n; i++) {
                if (Math.abs(A[i][h]) > maxVal) {
                    maxVal = Math.abs(A[i][h]);
                    iMax = i;
                }
            }
            if (maxVal < 1e-100) return null;
            let tmp = A[h]; A[h] = A[iMax]; A[iMax] = tmp;
                tmp = B[h]; B[h] = B[iMax]; B[iMax] = tmp;
            let f = A[h][h];
            for (let j = 0; j < n; j++) {
                A[h][j] /= f; B[h][j] /= f;
            }
            for (let i = 0; i < n; i++) {
                if (i == h) continue;
                f = -A[i][h];
                for (let j = 0; j < n; j++) {
                    if (j >= h) A[i][j] += A[h][j] * f;
                    B[i][j] += B[h][j] * f;
                }
            }
        }
        return B;
    }

    /** Optimize a function by gradient descent.
     *  @param p0 is a vector of numerical values that specify an initial
     *  point in the optimization space
     *  @param params is a client-defined object that is passed to the
     *  client-defined function, objFunc
     *  @param objFunc is a function that computes the value of the objective
     *  function at a point; its arguments include a vector that specifies a
     *  point and the params object
     *  @return a vector of values that minimizes objFunc
     */
    gradientDescent(p0, params, objFunc, targetValue, limit) {
        let p = p0.slice(); let pp = new Array(p.length);
        let pval = objFunc(p, params); let ppval = 0;
        let pvalHistory = new Array(10); pvalHistory.fill(pval);
        let wp = 0;
        let count = 0; const eps = 1e-12;
        let firstStep = 1; let maxStep = 0;
        while (pval > targetValue && count++ < limit) {
            // compute the gradient at p
            let delta = 1.e-10;
            let grad = new Array(p.length); let sum = 0;
            for (let i = 0; i < p.length; i++) {
                let di = Math.abs(p[i] * delta);
                p[i] += di;
                let dval = objFunc(p, params);
                p[i] -= di;
                grad[i] = (dval - pval) / di;
                sum += Math.abs(grad[i]);
            }
            let stepSize = firstStep;
            while (true) {
                for (let i = 0; i < p.length; i++)
                    pp[i] = p[i] - stepSize * (grad[i]);// / sum);
                ppval = objFunc(pp, params);
                if (ppval < pval) break;
                else if (stepSize < eps) {
                    return p;
                }
                stepSize /= 2;
            }
            [p, pp] = [pp, p]; pval = ppval;
            if (stepSize == firstStep)
                firstStep *= 2;
            maxStep = Math.max(stepSize, maxStep);
            if (stepSize <= maxStep)
                firstStep = Math.max(firstStep/2, 2*maxStep);
            if (count > 10 && pvalHistory[wp] - ppval < .01 * pval)
                break;
            pvalHistory[wp] = pval; wp = (wp+1) % 10;
        }
        return p;
    }

    /** Compute the discrete cosine transform of a vector.
     *  @param x is a vector of real numbers
     *  @return a vector of the same length representing the equivalent
     *  set of dct coefficients
     */
    dct(x, lo=0, hi=x.length) {
        let n = hi - lo;
        let a = new Array(n);
        for (let i = lo; i < hi; i++) {
            a[i] = 0;
            for (let j = lo; j < hi; j++) {
                a[i] += x[j] * Math.cos(i*(j+0.5)*Math.PI/n);
            }
        }
        return a;
    }

    /** Compute the inverse discrete cosine transform of a vector.
     *  @param a is a vector of real numbers
     *  @return a vector of the same length representing the equivalent
     *  set of inverset dct coefficients
     */
    idct(a, lo=0, hi=a.length) {
        let n = hi - lo;
        let x = new Array(n);
        for (let j = lo; j < hi; j++) {
            x[j] = a[lo] / 2;
            for (let i = lo+1; i < hi; i++) {
                x[j] += a[i] * Math.cos(i*(j+0.5)*Math.PI/n);
            }
            x[j] *= 2/n;
        }
        return x;
    }

    /** Apply a filter to vector.
     *  @param x is a vector of values
     *  @param fmin is the index of the first frequency coefficient to
     *  be included in the filtered version of the vector
     *  @param fmax-1 is the index of the last frequency coeeficient to
     *  be include in in the filtered version of the vector
     *  @param rake is an increasing vector of array indexes that defines
     *  sub-ranges to be excluded/included in the computation
     *  @return a filtered vector obtained by computing the dct coeficients
     *  zeroing the coefficients with index up to fmin-1 and from fmax up,
     *  then computing the inverse dct of the resulting coefficient vector
     */
    filter(x, fmin, fmax, rake=[0,x.length]) {
        let a = lib.dct(x, rake);
        a.fill(0, 0, fmin); a.fill(0, fmax);
        return lib.idct(a, rake);
    }

    /** Compute weighted sum of a set of vectors.
     *  @param weights is a vector of weights
     *  @param vecs is a vector of vectors
     *  @param id is an optional vector of identifiers;
     *  if present, weights[i] is associated with vecs[id[i]];
     *  if id is missing, weights[i] is associated with vecs[i].
     *  @param svx is an optional range of subvector indexes;
     *  if present, the result vector is computed only on the
     *  index values appearing in svx
     *  @return the vector obtained by weighting the selected
     *  vectors by the corresonding weights and summing them.
     */
    weightedSum(weights, vecs, id=null, svx=null) {
        if (id == null) id = lib.range(0, weights.length);
        if (svx == null) svx = lib.range(0, vecs[0].length);
        let result = new Array(svx.length);
        for (let j = 0; j < svx.length; j++) {
            let sum = 0;
            for (let i = 0; i < weights.length; i++) {
                sum += weights[i] * vecs[id[i]][svx[j]];
            }
            result[j] = sum;
        }
        return result;
    }

    /** List local minima of a vector.
     *  @param f is a vector representing a function on its indices
     *  @return a pair of vectors x, y where each x[i], y[i] pair defines
     *  a local minimum of f
     */
    localMinima(f) {
        let minx = []; let miny = [];
        for (let i = 0; i < f.length; i++) {
            if ((i == 0 && f[i] <= f[i+1]) ||
                (i == f.length-1 && f[i] <= f[i-1]) ||
                (f[i] < f[i-1] && f[i] <= f[i+1]) ||
                (f[i] <= f[i-1] && f[i] < f[i+1])) {
                minx.push(i); miny.push(f[i]);
            }
        }
        return [ minx, miny ];
    }

    /** Find points on the lower convex hull of a function.
     *  Uses Andrew's monotone chain algorithm.
     *  @param f is a vector representing a function on its indices
     *  @return a pair of vectors x, y where each x[i], y[i] pair defines
     *  a point on the lower convex hull of f.
     */
    lowerHull(f) {
        let x = new Array(f.length); x[0] = 0; x[1] = 1;
        let p = 2;
        for (let i = 2; i < f.length; i++) {
            while (p >= 2 && ((f[x[p-1]] - f[x[p-2]]) / (x[p-1] - x[p-2]))
                          >= ((f[i] - f[x[p-1]]) / (i - x[p-1])))
                p--;
            x[p++] = i;
        }
        x.length = p;
        let y = new Array(x.length);
        for (let i = 0; i < y.length; i++) y[i] = f[x[i]];
        return [x, y];
    }

    /** Interplate gaps in a sparse function.
     *  @param x is an increasing vector of integers, starting with 0
     *  @param y is a numeric vector of matching length; the x, y pairs
     *  define a function on integers
     *  @param gap is the maximum gap between successive x values in the
     *  returned function
     *  @return a pair of vectors [xf, yf] that matches [x, y] wherever
     *  [x, y] are defined, but fills in otherwise unfilled gaps by linear
     *  interpolation, so that there are never more than gap undefined
     *  successive points
     */
    fillSparseFunction(x, y, gap) {
        let xf = [x[0]]; let yf = [y[0]];
        for (let i = 1; i < x.length; i++) {
            if (x[i] <= x[i-1] + gap) {
                xf.push(x[i]); yf.push(y[i]); continue;
            }
            let delta = x[i] - x[i-1];
            let k = Math.ceil(delta / gap);    // # of gaps
            let g = delta / k;                // avg gap size
            for (let j = 1; j < k; j++) {
                let jg = Math.round(j * g);
                xf.push(x[i-1] + jg);
                yf.push(y[i-1] + (jg / delta) * (y[i]-y[i-1]));
            }
            xf.push(x[i]); yf.push(y[i]);
        }
        return [xf, yf];
    }

    /** Compute some statistics of a vector.
     *  @param x is a vector of numbers
     *  @return the mean of x, its standard deviation and its
     *  coefficient of variation
     */
    stats(x) {
        let n = x.length; let s = 0, ss = 0;
        for (let i = 0; i < n; i++) {
            s += x[i]; ss += x[i] * x[i];
        }
        mu = s/n; sigma = Math.sqrt((ss/n) - (mu*mu));
        return [ mu, sigma, sigma/mu ];
    }

    /** Compute a unit vector for a given vector.
     *  @param x is a vector of numbers
     *  @return the unit magnitude vector with the same direction as x
     */
    unitVector(x, rake) {
        let rx = this.applyRake(x, rake);
        return lib.vectorScale(rx, 1/lib.vectorMagnitude(rx));
        //return lib.vectorScale(x, 1/lib.vectorMagnitude(x, rake));
    }

    /** Transpose a 2d matrix.
     *  @param x is 2d array (technically, a vector of equal-length vectors)
     *  @return an array obtained by transposing the rows and columns of x
     */
    matrixTranspose(x) {
        let n = x.length;
        if (n == 0) return [];
        let m = x[0].length;
        let y = new Array(m);
        for (let j = 0; j < m; j++) {
            y[j] = new Array(n);
            for (let i = 0; i < n; i++) y[j][i] = x[i][j];
        }
        return y;
    }

    /** Compute the absolute error of two vectors
     *  @param a is a vector
     *  @param b is a second vector of the same length
     *  @return the pair [avg, max] where avg is the
     *  average of the absolute differences of a and b
     *  and max is the largest difference.
    absoluteError(a, b) {
        let sum = 0; let max = 0;
        for (let i = 0; i < a.length; i++) {
            if (a[i] == b[i]) continue;
            let err = Math.abs((a[i] - b[i]));
            sum += err; max = Math.max(max, err);
        }
        return [sum/a.length, max];
    };
     */

    /** Compute the relative error of two vectors
     *  @param a is a vector
     *  @param b is a second vector of the same length
     *  @return the pair [avg, max] where avg is the
     *  average of the relative differences of a and b
     *  and max is the largest difference.
    relativeError(a, b) {
        let sum = 0; let max = 0;
        for (let i = 0; i < a.length; i++) {
            if (a[i] == b[i]) continue;
            let err = Math.abs((a[i] - b[i]) / a[i]);
            sum += err; max = Math.max(max, err);
        }
        return [sum/a.length, max];
    };
     */

    /** Compute the squared error of two vectors
     *  @param a is a vector
     *  @param b is a second vector of the same length
     *  @return the sum of the squared errors.
     */
    squaredError(a, b, rake=[0,a.length]) {
        let sum = 0;
        for (let k = 1; k < rake.length; k += 2) {
            for (let i = rake[k-1]; i < rake[k]; i++) {
                let err = a[i] - b[i];
                sum += err*err;
            }
        }
        return sum;
    };

    /** Compute the normalized error of two vectors
     *  @param a is a vector
     *  @param b is a second vector of the same length
     *  @return the pair [avg, max] where avg is the
     *  the sum of the pairwise differences divided by
     *  the sum of the magnitudes of the values in a;
     *  max is the largest difference divided by the
     *  average magnitude of a.
    normalizedError(a, b) {
        let sum = 0; let max = 0; let suma = 0;
        for (let i = 0; i < a.length; i++) {
            if (a[i] == b[i]) continue;
            let err = Math.abs((a[i] - b[i]));
            sum += err; max = Math.max(max, err);
            suma += Math.abs(a[i]);
        }
        return [sum/suma, max * a.length / suma];
    };
     */

    /** Parse a vector of floating point values.
     *  @param buf is a string representing a f.p. vector
     *  @return array of values.
     */
    parseFloatVector(buf) {
        let i, j;
        j = buf.indexOf("[");
        if (j < 0) return [];
        i = j+1; j = buf.indexOf(",",i);
        let vec = [];
        while (j >= 0) {
            let x = parseFloat(buf.slice(i,j));
            vec.push(x);
            i = j+1; j = buf.indexOf(",",i);
        }
        j = buf.indexOf("]",i);
        let x = parseFloat(buf.slice(i,j));
        vec.push(x);
        return vec;
    }

    /** Create a formatted string representation of a number.
     *  @param val is the number to be formatted
     *  @prec is the desired precision.
     *  @return a string representation of val using fixed format
     *  or exponnential, depending on which is more appropriate
     */
    format(val, prec) {
        if (Math.abs(val) < 1e-20) return "0";
        let i = 0; let x = Math.abs(val);
        if (x >= 1) {
            while (x >= 1 && i <= 8) { x /= 10; i++; }
            return (i < 8 ? val.toFixed(Math.max(0, prec - i))
                          : val.toExponential(prec-1));
        } else {
            while (x < 1 && i <= 4) { x *= 10; i++; }
            return (i <= 3  ? val.toFixed(prec + (i-1))
                            : val.toExponential(prec-1));
        }
    }
    
    /** Return the current time in milliseconds since the epoch. */
    currentTime() {
        return (new Date()).getTime();
    }

};
