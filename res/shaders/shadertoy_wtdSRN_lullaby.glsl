
// -----------------------------------------------------
// lullaby by nabr
// https://www.shadertoy.com/view/wtdSRN
// License Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// https://creativecommons.org/licenses/by-nc/4.0/
// -----------------------------------------------------
// -----------------------------------------------------
// bandcamp:
// name your price, type ZER0$ for free download
// https://tolka-nabroski.bandcamp.com/album/lullaby
// -----------------------------------------------------



#define PART 1 // 1|2

float intfract(int s,float n);
#define tau 6.2831853
#define size(_a)_a.length()

float synth(int rsamp)
{
    #if PART == 1
    float ft =  (float(rsamp) / iSampleRate * .125);
    // init
    float s0=0., s1=0., sm=0.;
    const float p[] = float[]( 337.5, 436.5, 384., 600., 0., 150., 216. );
    
    // main carrier 
    s0 += sin(tau * intfract(rsamp, iSampleRate / 450. ));
    s0*= .2 * exp(-10. * (cos( ( 1.625 * ft - .141592  )  )*.5+.5) );
    
  	// melody
    float bt = (2. * ft);
	s1 += sin(tau * intfract(rsamp, iSampleRate / p[int( bt) % size(p)] ) );
	float s1a = min(sqrt(sqrt(1. - fract(bt))), 10. * fract(bt));
	s1a = s1a * s1a * s1a * s1a;
	s1 = s1a * s1;
    
    // wavesphaping
    sm += .75 * (s0+s1);
    float sma = min(sqrt(sqrt(1.-fract(-24.*ft))),10.*fract(-24.*ft));
	sma = sma * sma * sma * sma;
	sm = sma * sm;
    sm = clamp(sm, -1., 1.);
    
  	// -------- out
    return sm;
    
    
    // -------- PART II
    #else
    float dr = 0., adr = 0.;
	float ft = (float(rsamp)/iSampleRate * .125);
	int np = int(2. * ft) % 7;
	int dsamp = rsamp /2;
	float btdr = 4. * ft;
	float fd = mod(btdr, 1.);
	float a = min(sqrt(sqrt(1. - fd)), 100. * fd);
	float hz = float[]( 400., 0., 600., 350., 0., 595., 0.)[int(btdr) % 7];
	// -------- flageolettton
	if (np == 1 || np == 3 || np == 5)
	{
		
        dr += sin(tau * intfract(dsamp, iSampleRate / hz ) );
        adr += .5 - sin(tau * intfract(dsamp, iSampleRate / (.75*hz) ) );
		adr *= atan(1. - fd, 100. * fd);
		dr += sin(.125 - (adr - cos(adr + dr)));
		dr *= (a*a*a*a);
		dr *= atan(1. - fd);
	}
	return dr;
    
    #endif
}

vec2 mSound(int samp)
{
    
    #if PART == 1
    float s0 = synth(samp);
    float s1 = s0, s2 = s0;
    // dry signal
    vec2 ds = vec2(.67 * s0 , .62 * s0);
    //delay/echos
    int nt = 983;
    int k[] = int[](80671,73907,86813,95279,87421,102859,78517,68581,83341,180811,174721,181717);
    int nrs =size(k);
    for(int ii=0;ii<nrs;++ii)
    {
    	s1+=synth(nt+samp);
        s2+=synth(2*nt+samp);
        nt+=k[ii%nrs];// k[]/44100.
    }
    return  (1./float(nrs)) * vec2(s1 , s2) + ds;

    
    // -------- PART II
    #else
    float s0 = synth(samp);
    float s1 = s0, s2 = s0;
    // dry signal
    vec2 ds = vec2(.67 * s0 , .62 * s0);
    //delay/echos
    int nt = 983;
    int k[9] = int[9](80671, 73907, 86813, 95279, 87421, 102859, 78517, 68581, 83341);
    int nrs =size(k);
    int nt0 = 983 , nt1 = 1543 ;
    for(int ii=nrs;ii>0;--ii)
    {
        nt0 += 2 * k[ii%nrs];
        s1 += (synth(nt0 + samp));
        nt1 += k[ii%nrs] ;
        s2 += (synth(nt1 + samp));
    }
    return  .2 * vec2(s1 , s2);
        
    
    #endif
}

float intfract(int s,float n)
{
    //Explanation and copyright : shadertoy.com/view/4ltfRN
    int sn=int(n),a=s%sn,b=(s/sn);
    const float MAXF=float(uint(-1))+1.;
    return fract((float(a)/n)+float((uint(b)*uint(MAXF*float(sn)/n)))/MAXF);
}
vec2 mainSound( in int s,float time)
{
   // int block = int((iBlockOffset+0.5)*iSampleRate/(512.*512.));
    // int s = 512*512*block + 512*int(gl_FragCoord.y) + int(gl_FragCoord.x);
    return mSound(s);
}