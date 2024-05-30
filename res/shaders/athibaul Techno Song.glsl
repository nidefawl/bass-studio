
/*
    Rights to all modifications herein are hereby assigned to Alexis Thibault in
    hopes that they are found useful and may be reintegrated freely.
    - Theron Tarigo (ttg)
*/


// The code below is split into several parts.
// UTILS - Constants and hash functions and stuff
// WAVEFORMS - Basic noise and tone generators
// INSTRUMENTS - Stuff that makes notes
// PHRASES AND SONG PARTS - What to play, how to play it, up to the final mix.


///////////////////////////////
/////////// UTILS /////////////
///////////////////////////////

#define TAU 6.2831855

float intfract(int s, float n) {
  // Implements fract(s/n)
  // Explanation and copyright: shadertoy.com/view/4ltfRN
  // Given sample number and period, calculate phase with high accuracy.
  // Uses integer overflow for modulo.
  int sn = int(n), a = s%sn, b = (s/sn);
  const float MAXF = float(uint(-1))+1.;
  return fract(  (float(a)/n) + float((uint(b)*uint(MAXF*float(sn)/n)))/MAXF );
}

float oscfr(int s, float f) {
  return intfract(s, iSampleRate/f);
}

float oscph(int s, float f) {
  return TAU*oscfr(s,f);
}

vec2 oscfr(int s, vec2 f) {
  return vec2(oscfr(s,f.x),oscfr(s,f.y));
}

vec2 oscph(int s, vec2 f) {
  return TAU*oscfr(s,f);
}

vec2 osc(int s,float f) {float p=oscph(s,f);return vec2(cos(p),sin(p));}
float osccos(int s,float f) {return osc(s,f).x;}
float oscsin(int s,float f) {return osc(s,f).y;}

int S(float t) {return int(round(t*iSampleRate));}
ivec4 S(vec4 t) {return ivec4(round(t*iSampleRate));}
float T(int s) {return float(s)/iSampleRate;}
vec4 T(ivec4 s) {return vec4(s)/iSampleRate;}

int modsi(int a,int b){ int m=(a<0?~a:a)%b; return a<0?b-1-m:m; }
ivec4 modsi(ivec4 a,int b){
  ivec4 r; for(int i=0;i<4;i++) r[i]=modsi(a[i],b); return r; }

int smod(int samp,float offset,float period) {return modsi(samp-S(offset),S(period));}
ivec4 smod(int samp,vec4 offset,float period) {return modsi(samp-S(offset),S(period));}

// Convert MIDI note number to cycles per second
#define midicps(n) (440.*exp(log(2.)*(n-69.)/12.))

float rand(float p)
{
    // Hash function by Dave Hoskins
    // https://www.shadertoy.com/view/4djSRW
    p = fract(p * .1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

vec2 rand2(float p)
{
    // Hash function by Dave Hoskins
    // https://www.shadertoy.com/view/4djSRW
	vec3 p3 = fract(vec3(p) * vec3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx+p3.yz)*p3.zy);
}

////////////////////////////////////
/////////// WAVEFORMS //////////////
////////////////////////////////////

float noise(float s){
    // Noise is sampled at every integer s
    // If s = t*f, the resulting signal is close to a white noise
    // with a sharp cutoff at frequency f.
    
    // For some reason float(int(x)+1) is sometimes not the same as floor(x)+1.,
    // and the former produces fewer artifacts?
    int si = int(floor(s));
    float sf = fract(s);
    sf = sf*sf*(3.-2.*sf); // smoothstep(0,1,sf)
    //sf = sf*sf*sf*(sf*(sf*6.0-15.0)+10.0); // quintic curve
    // see https://iquilezles.org/articles/texture
    return mix(rand(float(si)), rand(float(si+1)), sf) * 2. - 1.;
}

vec2 noise2(float s){
    int si = int(floor(s));
    float sf = fract(s);
    sf = sf*sf*(3.-2.*sf); // smoothstep(0,1,sf)
    return mix(rand2(float(si)), rand2(float(si+1)), sf) * 2. - 1.;
}

float coloredNoise(int s, float fc, float df)
{
    // Noise peak centered around frequency fc
    // containing frequencies between fc-df and fc+df
    
    // Assumes fc is an integer, to avoid problems with sin(large number).
    
    // Modulate df-wide noise by an fc-frequency sinusoid
    //float n1 = noise(t*df);
    //float n2 = noise(t*df - 100000.);
    //vec2 modul = vec2(cos(TAU*fc*t), sin(TAU*fc*t));
    // original:
    // return sin(TAU*fc*fract(t))*noise(t*df);
    // was fc*fract(t) a mistake? : fract(fc*t)
    return oscsin(s,fc)*noise(T(s)*df);
}

vec2 coloredNoise2(int s, float fc, float df)
{
    // Noise peak centered around frequency fc
    // containing frequencies between fc-df and fc+df
    vec2 noiz = noise2(T(s)*df);
    vec2 modul = osc(s,fc);
    return modul*noiz;
}


float window(float a, float b, float t)
{
    return smoothstep(a, (a+b)*0.5, t) * smoothstep(b, (a+b)*0.5, t);
}

float formantSin(float phase, float form)
{
    // Inspired by the wavetable "formant" option
    // in software synthesizer Surge (super cool freeware synth!)
    phase = fract(phase);
    phase = min(phase*form, 1.);
    return sin(TAU*fract(phase));
}
vec2 formantSin2(vec2 phase, vec2 form)
{
    // Inspired by the wavetable "formant" option
    // in software synthesizer Surge (super cool freeware synth!)
    phase = fract(phase);
    phase = min(phase*form, 1.);
    return sin(TAU*fract(phase));
}

vec2 lpfSaw(int samp, vec2 f, float fc, float Q)
{
    // Low-pass-filtered sawtooth wave
    // arguments are samp, frequency, cutoff frequency, and resonance quality factor
    vec2 omega_c = 2.*3.14159*fc/f; // relative
    vec2 t2 = oscfr(samp,f);
    // Compute the exact response of a second order system with those parameters
    // (value and derivative are continuous)
    // It is expressed as
    // 1 - 2t + A exp(-omega_c*t/Q) * cos(omega_c*t+phi)
    // We need to compute the amplitude A and phase phi.
    vec2 alpha = omega_c/Q, beta=exp(-alpha), c = cos(omega_c), s = sin(omega_c);
    vec2 tanphi = (alpha*beta*c + beta*omega_c*s - alpha) / (omega_c + alpha*beta*s - beta*omega_c*c);
    // We could use more trigonometric identities to avoid computing the arctangent, but whatever.
    vec2 phi = atan(tanphi);
    vec2 A = -2./(cos(phi) - beta*cos(omega_c+phi));
    
    vec2 v = 1.-2.*t2 + A*exp(-alpha*t2) * cos(omega_c*t2+phi);
    return v;
}

float lpfSaw(int samp, float f, float fc, float Q)
{
    return lpfSaw(samp,vec2(f),fc,Q).x;
}


///////////////////////////////////
//////// INSTRUMENTS //////////////
///////////////////////////////////

vec2 hat1(int s)
{
    // Smooth hi-hat, almost shaker-like
    float t = T(s);
    return coloredNoise2(s, 10000., 5000.) * smoothstep(0.,0.02,t) * smoothstep(0.06,0.01,t) * 0.1;
}

vec2 hat2(int s, float fc)
{
    // Short hi-hat with tuneable center frequency
    float t = T(s);
    return coloredNoise2(s, fc, fc-1000.) * smoothstep(0.,0.001,t) * smoothstep(0.03,0.01,t) * 0.1;
}

vec2 snare1(int s)
{
    // Composite snare
    float t = T(s);
    float body = (oscsin(s,250.) + oscsin(s,320.)) * smoothstep(0.1,0.0,t) * 1.;
    vec2 timbre = coloredNoise2(s, 1000., 7000.) * exp(-12.*t) * smoothstep(0.5,0.,t) * 8.;
    vec2 sig = (body+timbre) * smoothstep(0.,0.001,t);
    sig = sig/(1.+abs(sig)); // distort
    sig *= (1. + smoothstep(0.02,0.0,t)); // increase transient
    return sig * 0.1;
}

vec2 snare2(int s)
{
    // Basic noise-based snare
    float t = T(s);
    float noi = coloredNoise(s, 4000., 1000.) + coloredNoise(s, 4000., 3800.) + coloredNoise(s,8000.,7500.) * 0.5;
    float env = smoothstep(0.,0.001,t) * smoothstep(0.2,0.05,t);
    env *= (1. + smoothstep(0.02,0.0,t)); // increase transient
    env *= (1. - 0.5*window(0.02,0.1,t)); // fake compression
    vec2 sig = vec2(noi) * env;
    sig = sig/(1.+abs(sig));
    return sig * 0.1;
}

float kick1(int s)
{
    // Composite kick
    
    // Kick is composed of a decaying sine tone, and a burst of noise,
    // all of it distorted and shaped with a nice envelope.
    
    // frequency is assumed to be f0 + df*exp(-t/dftime);
    float t = T(s);
    float f0 = 50., df=500., dftime=0.02;
    float phase = TAU * (f0*t - df*dftime*exp(-t/dftime));
    float body = sin(phase) * smoothstep(0.15,0.,t) * 2.;
    float click = coloredNoise(s, 8000., 2000.) * smoothstep(0.01,0.0,t);
    //float boom = sin(f0*TAU*t) * smoothstep(0.,0.02,t) * smoothstep(0.15,0.,t);
    float sig = body + click;
    sig = sig/(1.+abs(sig));
    //sig += boom;
    sig *= (1. + smoothstep(0.02,0.0,t)); // increase transient
    sig *= (1. + window(0.05,0.15,t)); // increase tail
    return sig * 0.2;
}

vec2 bass1(int s, float f, float cutoff)
{
    // Composite bass
    // (I'm very happy about this one!)
    
    // "Cutoff" is not actually the cutoff frequency of a filter,
    // but it controls the amount of high frequencies
    // we bring in using the "formantSin" waveform.
    float t = T(s);
    cutoff *= exp(-t*5.);
    float formant = max(cutoff/f, 1.);
    // Pure sine tone
    float funda = oscsin(s,f);
    // Phase-modulated sine gives more "body" to the sound
    float body = sin(oscph(s,2.*f) + (0.2*formant)*oscsin(s,f));
    // Gritty attack using a truncated sinusoid waveform
    // (dephased for stereo effect)
    vec2 highs = formantSin2(oscfr(s,f) + vec2(0,0.5), vec2(formant)) * exp(-t*10.);
    vec2 sig = body + highs + funda;
    // Two-rate envelope with a strong transient and long decay
    sig *= (2.*exp(-t*20.) + exp(-t*2.));
    sig *= (1. + 0.3*smoothstep(0.05,0.0,t)); // increase transient
    
    // Finally, add some distortion
    //sig = sig / (1. + abs(sig)); // Feel free to try how this one sounds.
    sig = sin(sig); // This one gives lovely sidebands when pushed hard.
    return sig * 0.1;
}

vec2 pad1(int s, vec4 f, float fc, float Q)
{
    // Filtered sawtooth-based pad, playing four-note chords
    
    // f: frequencies of the four notes
    // fc, Q: cutoff frequency and quality factor of the 12dB/octave lowpass filter
    vec2 sig = vec2(0);
    sig += lpfSaw(s, f.x+vec2(-2,2), fc, Q);
    sig += lpfSaw(s, f.y+vec2(1.7,-1.7), fc, Q);
    sig += lpfSaw(s, f.z+vec2(-0.5,0.5), fc, Q);
    sig += lpfSaw(s, f.w+vec2(1.5,-1.5), fc, Q);
    return sig * 0.02;
}

vec2 arp1(int s, vec4 f, float fc, float dur)
{
    // Plucky arpeggiator, playing 16th notes.
    
    // dur: decay time of the notes (amplitude and filter)
    vec2 sig = vec2(0);
    ivec4 ss = smod(s,vec4(0,0.125,0.25,0.375), 0.5);
    vec4 ts = T(ss);
    sig += lpfSaw(s, f.x, fc*exp(-ts.x/dur), 10.) * smoothstep(0.0,0.01,ts.x) * exp(-ts.x/dur);
    sig += lpfSaw(s, f.y, fc*exp(-ts.y/dur), 10.) * smoothstep(0.0,0.01,ts.y) * exp(-ts.y/dur);
    sig += lpfSaw(s, f.z, fc*exp(-ts.z/dur), 10.) * smoothstep(0.0,0.01,ts.z) * exp(-ts.z/dur);
    sig += lpfSaw(s, f.w, fc*exp(-ts.w/dur), 10.) * smoothstep(0.0,0.01,ts.w) * exp(-ts.w/dur);
    return sig * 0.04;
}

vec2 marimba1(int s, float f)
{
    // Simple phase-modulation based marimba
    float t = T(s);
    vec2 sig = vec2(0);
    // Super basic marimba sound
    sig += sin(oscph(s,f) + exp(-50.*t)*sin(7.*oscph(s,f))) * exp(-5.*t) * step(0.,t);
    // Fake reverb effect: long-decay, stereo-detuned fundamental
    sig += sin(oscph(s,f+vec2(-2,2))) * exp(-1.5*t) * 0.5;
    return vec2(sig) * 0.05;
}

vec2 pad2(int s, vec4 f, float fres)
{
    // Four-note, phase-modulation-based pad.
    
    // fres: center frequency of the faked "spectral aliasing"
    
    vec2 sig = vec2(0);
    // Index of modulation
    // https://en.wikipedia.org/wiki/Frequency_modulation#Modulation_index
    vec4 iom1 = 2.+0.5*sin(T(s) + vec4(0,1,2,3));
    // Play an octave lower than asked
    f *= 0.5;
    // Modulator has frequency 2f -> odd harmonics only
    sig += sin(oscph(s,f.x) + iom1.x * sin(2.*oscph(s,f.x+vec2(-1,1)))) * vec2(1,0);
    sig += sin(oscph(s,f.y) + iom1.y * sin(2.*oscph(s,f.y+vec2(-1.2,0.8)))) * vec2(0.7,0.3);
    sig += sin(oscph(s,f.z) + iom1.z * sin(2.*oscph(s,f.z+vec2(-0.5,1.5)))) * vec2(0.3,0.7);
    sig += sin(oscph(s,f.w) + iom1.w * sin(2.*oscph(s,f.w+vec2(-1.3,0.7)))) * vec2(0,1);
    
    // Fake spectral aliasing, to add some high-end
    vec2 warped = vec2(0);
    warped += sin(oscph(s,fres) + 5.*sin(oscph(s,f.x))) * vec2(1,0);
    warped += sin(oscph(s,fres) + 5.*sin(oscph(s,f.y))) * vec2(0.7,0.3);
    warped += sin(oscph(s,fres) + 5.*sin(oscph(s,f.z))) * vec2(0.3,0.7);
    warped += sin(oscph(s,fres) + 5.*sin(oscph(s,f.w))) * vec2(0,1);
    
    // Mix to taste
    sig = (sig + 0.01*warped) * 0.02;
    // Reduce stereo image
    sig = mix(sig.xy, sig.yx, 0.1);
    return sig;
}


////////////////////////////////////////////
/////// PHRASES AND SONG PARTS /////////////
////////////////////////////////////////////


float leadphrasenote(int s)
{
    // Four-bar lead synth phrase in the final chorus
    // MIDI note number (or 0. if silence)
    float t = T(s);
    float note =
        (t<0.5) ? 69. : (t<1.) ? 71. : (t<1.5) ? 72. : (t<1.75) ? 76. :
        (t<3.0) ? 74. : (t<3.25) ? 0. : (t<3.5) ? 72. : (t<3.75) ? 74. :
        (t<5.5) ? 76. : (t<5.75) ? 79. : (t<7.5) ? 71. : 0.;
    return note;
}

vec2 leadphrase1(int s, float fc)
{
    // Four-bar lead synth phrase in the final chorus
    
    float note = leadphrasenote(s);
    // Add some vibrato
    float t = T(s);
    float vibStrength = window(2.,3.,t) + window(4.,5.5,t) + window(6.,8.,t);
    float f = midicps(note + vibStrength*0.01*oscsin(s,5.)/(T(s)+0.1));
    // Cut silence
    float env = (note > 0.) ? 1. : 0.;
    
    // "Super-saw" lead
    vec2 sig = lpfSaw(s, f+vec2(-2,2), fc, 1.);
    sig += lpfSaw(s, f+vec2(3.2,-3.2), fc, 1.);
    sig += lpfSaw(s, f, fc, 1.);
    
    // Distort
    sig *= 2.;
    sig = sig/(1.+abs(sig));
    
    return sig * 0.05 * env;
}

vec2 leadchorus(int s, float fc)
{
    // Four-bar lead synth phrase in the final chorus
    // Add delay effect
    vec2 sig = leadphrase1(s, fc);
    sig = mix(sig, sig.yx, 0.3);
    sig += leadphrase1(smod(s,0.25,8.), fc*0.7).yx * vec2(0.5,-0.5);
    sig += leadphrase1(smod(s,1., 8.), 1000.) * 0.5;
    return sig;
}

vec2 basschorus(int s, float fc)
{
    // Bass of the final chorus:
    // Simply play the fundamental of each bar, with octave jumps
    
    // Every second 8th note is an octave above
    float t = T(s);
    float octave = 12.*step(0.25,mod(t,0.5));
    // Fundamental of each of the four bars
    float note = (t<2.) ? 69.-36.+octave : 
                 (t<4.) ? 62.-36.+octave :
                 (t<6.) ? 60.-36.+octave :
                 67.-36.+octave;
    
    int s1 = smod(s,0., 0.25);
    vec2 sig = bass1(s1, midicps(note), fc);
    
    return sig;
}

vec2 padchorus(int s, float fc, float Q)
{
    // Pad part for the final chorus
    // Simply play the (slightly rich) chords
    // ||: Am(add9) | Dm7 | C(add9) | G(add9) :||
    float t = T(s);
    vec4 chord = (t<2.) ? vec4(57,60,64,71) : (t<4.) ? vec4(57,62,65,72) : (t<6.) ? vec4(60,62,64,67) : vec4(59,62,67,69);
    
    vec2 pad = pad1(s, midicps(chord), fc, Q);
    return pad;
}


vec2 arpchorus(int s, float fc, float dur)
{
    // Arpeggiator part for the final chorus
    // Simply arpeggiate the four chords
    float t = T(s);
    vec4 chord = (t<2.) ? vec4(57,60,64,71) : (t<4.) ? vec4(57,62,65,72) : (t<6.) ? vec4(60,62,64,67) : vec4(59,62,67,69);
    vec2 arp = arp1(s, midicps(chord+12.), fc, dur);
    return arp;
}


vec2 fullChorus(int samp)
{
    // Full mix for the final chorus
    samp = smod(samp, 0., 8.);
    vec2 v = vec2(0);
    
    // Percussions (with a slight 16th-note swing)
    v += hat1(smod(samp, 0., 0.25)) * vec2(0.8,1.0);
    v += hat1(smod(samp, 0.14, 0.25)) * vec2(0.3,-0.2);
    v += snare1(smod(samp, 0.5, 1.));
    v += kick1(smod(samp, 0., 0.5));
    
    // Low-frequency oscillator on a macro control
    float cutoff = 300. + 200.*oscsin(samp,1./TAU);
    
    float t = T(smod(samp,0., 0.5));
    // Another LFO for fake sidechain compression ("pumping" effect)
    float pumping = mix(smoothstep(0.0,0.25,t), smoothstep(0.0,0.5,t), 0.2);
    
    v += basschorus(smod(samp,0.,8.), cutoff) *mix(pumping, 1.,0.3);
    
    vec2 pads = padchorus(smod(samp,0., 8.), 8000.-1000.*oscsin(samp,1./TAU), 2.);
    pads *= mix(pumping, 1., 0.1);
    v += pads;
    
    // A third LFO to vary the note length of the arpeggiator
    float dur = 0.2 * exp(0.2*oscsin(samp,0.6/TAU));
    vec2 arp = arpchorus(smod(samp,0., 8.), 5000.-1000.*osccos(samp,0.7/TAU), dur);
    v += arp * mix(pumping, 1.,0.2);
    
    v += leadchorus(smod(samp,0.,8.), 10000.) * mix(pumping,1.,0.5);
    
    return v;
}

vec2 padPhraseVerse(int samp, float fc)
{
    // Pad during the verse: play three chords in four bars
    // ||: Am(add11) | FMaj7 | Em7 | Em7 :||
    int s = smod(samp,0., 8.);
    float t = T(s);
    vec4 chord = (t<2.) ? vec4(57,60,62,64) : (t<4.) ? vec4(53,57,60,64) : vec4(52,55,62,64);
    // Smoothe out the transitions from one chord to the next,
    // as they are not masked by percussion.
    float env = 1. - window(-0.1,0.1,t) - window(1.9,2.1,t) - window(3.9,4.1,t) - window(7.9,8.1,t);
    // Add some movement with volume automation
    env *= 1. + 0.2*window(0.25,0.5,mod(t,0.5));
    return pad1(s, midicps(chord), fc*0.7, 2.) * env;
}

vec2 padVerse(int samp, float fc)
{
    // Verse pad with delay effect
    return padPhraseVerse(samp, fc) + 0.5*padPhraseVerse(samp-S(0.5),fc).yx + 0.2*padPhraseVerse(samp-S(1.5),fc);
}

vec2 marimbaVerse(int s, float fc)
{
    // Marimba part for the verse:
    // just a few notes, always the same.
    vec2 v = vec2(0);
    v += marimba1(smod(s,0.00,8.), midicps(72.));
    v += marimba1(smod(s,0.75,8.), midicps(71.));
    v += marimba1(smod(s,1.50,8.), midicps(69.));
    v += marimba1(smod(s,2.25,8.), midicps(64.));
    v += marimba1(smod(s,7.50,8.), midicps(69.));
    v += marimba1(smod(s,7.75,8.), midicps(71.));
    return v;
}

vec2 arpVerse(int samp, float fc, float dur)
{
    // Verse arpeggiator: just arpeggiate the chords
    // (different notes than the pad this time).
    // Cutoff frequency and note duration will be varied for tension.
    int s = smod(samp,0., 8.);
    float t = T(s);
    vec4 chord = (t<2.) ? vec4(57,64,69,71) : (t<4.) ? vec4(57,64,65,72) : vec4(59,64,69,74);
    return arp1(s, midicps(chord), fc, dur);
}

vec2 fullVerse(int samp)
{
    vec2 v = vec2(0);
    // Cutoff frequency: dark sound initially,
    // but with a riser in the last four bars.
    float fc = 400. - 100.*osccos(samp,1./TAU) + 10000. * pow(clamp((T(samp)-24.)/(32.-24.),0.,1.), 4.);
    v += padVerse(samp, fc) * 0.5;
    v += marimbaVerse(samp, fc);
    if(samp > S(16.))
    {
        // Arpeggiator comes in after 8 bars, and note duration increases
        // during the riser.
        float dur = mix(0.05,0.5, smoothstep(24.,32.,T(samp)));
        v += arpVerse(samp, fc, dur) * smoothstep(16.,18.,T(samp));
    }
    return v;
}

vec2 bassDrop1(int samp)
{
    // Groovy four-bar phrase of the bass during the drop.
    
    // (In fact, it is the only part of this song with
    // some melodic/rhythmic complexity and variation.
    // The rest is extremely mechanical.)
    
    vec2 v = vec2(0);
    
    samp = smod(samp,0., 8.);
    
    int sx = samp / S(0.125); // sixteenth note number
    int st = samp % S(0.125);
    bool isShort = true; // True for 16th note, false for 8th note
    vec2 nn = vec2(0.,0.); // note number, trigger short note
    nn = (sx == 0 || sx == 5 || sx==8) ? vec2(33,1) : 
         (sx == 2) ? vec2(48,1) :
         (sx == 3) ? vec2(45,1) :
         (sx == 14) ? vec2(35,1) :
         (sx == 15 || sx == 35) ? vec2(36,1) :
         (sx == 16 || sx == 21 || sx == 24 || sx == 30 || sx == 31) ? vec2(26,1) :
         (sx == 18) ? vec2(41,1) :
         (sx == 19) ? vec2(38,1) :
         (sx == 32 || sx == 37 || sx == 38 || sx == 40) ? vec2(24,1) :
         (sx == 34) ? vec2(40,1) :
         (sx == 46) ? vec2(28,1) :
         (sx == 47) ? vec2(29,1) :
         (sx == 48 || sx == 53 || sx == 54 || sx == 56 || sx == 57) ? vec2(31,1) :
         (sx == 50) ? vec2(47,1) :
         (sx == 51 || sx == 58) ? vec2(43,1) :
         (sx == 60 || sx == 61) ? vec2(32,1) :
         (sx == 62) ? vec2(44,1) :
         vec2(0,0);
    
    
    if(sx == 30 || sx == 56 || sx == 60)
    { // First half of 8th notes
        isShort = false;
    }
    if(sx == 31 || sx == 57 || sx == 61)
    {  // Second half of 8th notes
        st += S(0.125);
        isShort = false;
    }
    
    
    float fc = 400. + 50.*oscsin(samp,1.);
    v += bass1(st, midicps(nn.x), fc) * nn.y;
    
    // Decay end of note to avoid clicks
    if(isShort) v *= smoothstep(0.125,0.12,T(st));
    else v *= smoothstep(0.125,0.12,T(st)-0.125);
    
    return v;
}

vec2 padDrop1(int samp, float fres)
{
    // Pad part for the drop : uses pad2 (the phase-modulation based pad)
    vec2 v = vec2(0);
    
    int s = smod(samp,0., 8.);
    // Very sparse choice of notes.
    // Chord transitions happen after the start of the bar.
    float t = T(s);
    vec4 chord = (t < 2.75) ? vec4(69,72,69,72) : 
    (t < 4.75) ? vec4(69,72,69,74) : (t < 6.75) ? vec4(69,72,67,72) : vec4(69,72,69,71);
    // Funky automation to avoid boredom
    float env = (0.05 + window(0.,4.,t) + window(4.,8.,t)) * exp(-5.*mod(-t, 0.25));
    v += pad2(samp, midicps(chord), fres) * env;
    
    return v;
}

vec2 fullDrop1(int samp)
{
    // Full mix of the bass drop.
    vec2 v = vec2(0);
    int s = smod(samp,0., 0.5);
    // Fake sidechain compression again
    float pumping = mix(smoothstep(0.0,0.25,T(s)), smoothstep(0.0,0.5,T(s)), 0.2);
    // Hi-hat timbre rises from "dull" to "harsh"
    float fhat = 5000. + 3000.*smoothstep(24.,32.,T(samp));
    
    v += bassDrop1(samp) * mix(pumping, 1., 0.8);
    v += kick1(smod(samp,0., 0.5) + S(0.008));
    
    v += padDrop1(samp, 8000.) * mix(pumping, 1., 0.05);
    
    if(samp > S(8.))
    {
        // Snare comes in after 4 bars.
        v += snare2(smod(samp,0.5, 1.));
    }
    if(samp > S(16.))
    {
        // Hi-hat comes in after 8 bars
        // Short hi-hat sound with fast attack and decay. Slight swing.
        v += hat2(smod(samp,0., 0.25), fhat) * vec2(0.8,1.0) * 0.7;
        v += hat2(smod(samp,0.14, 0.25), fhat) * vec2(0.3,-0.2) * 0.7;
    }
    return v;
}

vec2 fermata1(int samp)
{
    // 2-bar fermata after verse
    vec2 v = vec2(0);
    // Let the last marimba note decay
    v += marimba1(samp, midicps(69.));
    // Let the pad go from bright to dark
    float time = T(samp);
    float fc = 10000. * exp(-5.*smoothstep(0.,4.,time));
    v += pad1(samp, midicps(vec4(57,60,62,64)), fc, 2.) * smoothstep(0.,0.1,time) * smoothstep(4.,0.,time);
    
    // Riser before drop:
    // Lots of low-frequency noise + a bit of high-frequency
    vec2 noise = (coloredNoise2(samp, 250., 250.) + 0.1*coloredNoise2(samp, 8000., 2000.)) * 0.2 * exp(-6.*smoothstep(4.,1.,time)) * smoothstep(4.,3.99,time);
    v += noise;
    
    return v;
}


vec2 teller1(int samp)
{
    // 1-bar riser before chorus
    vec2 v = vec2(0);
    int s = smod(samp,0., 0.5);
    float time = T(samp);
    float fc = 10000.*exp(2.*(time-2.));
    // Noise riser
    vec2 riser = coloredNoise2(samp, fc*0.3, fc*0.3);
    v += riser * smoothstep(0.,2.,time) * 0.3 * exp((time-2.)*3.);
    // Announce the "middle A" played by the lead synth on the chorus
    vec2 teller = pad1(samp, midicps(vec4(69)), fc, 2.);
    v += teller;
    return v;
}


vec2 verseTeller(int samp)
{
    // Pre-announce the first note played by the marimba.
    float fC5 = midicps(72.);
    return (sin(oscph(samp,fC5+vec2(-2,2))) + 0.5*sin(oscph(samp,fC5+vec2(3,-3)))) * 0.1 * exp(-5.*(2.-T(samp)));
    
}

vec2 fullSong(int samp)
{
    // Combine all parts of the song into a structured whole.
    
    vec2 v = vec2(0);
    
    if(0<samp && samp < S(2.))
    {
        v += verseTeller(samp);
    }
    
    samp -= S(2.);
    
    if(0 < samp && samp < S(32.))
    {
        v += fullVerse(samp);
    }
        
    samp -= S(32.);
    
    if(0 < samp && samp < S(4.))
    {
        v += padVerse(samp, 10000.) * smoothstep(0.5,0.,T(samp));
        v += fermata1(samp);
    }
    
    samp -= S(4.);
    
    if(0 < samp && samp < S(32.))
    {
        v += fullDrop1(samp);
    }
    
    samp -= S(32.);
    
    if(0 < samp && samp < S(4.))
    {
        v += bass1(samp, midicps(33.), 400.);
        v += pad2(samp, midicps(vec4(69,71,69,72)), 8000.) * (0.5 + 0.3*osccos(samp,1.)) 
             * smoothstep(0.,0.5,T(samp)) * smoothstep(4.,0.,T(samp));
        v += verseTeller(samp-S(2.));
    }
    
    samp -= S(4.);
    
    if(0 < samp && samp < S(16.))
    {
        v += fullVerse(samp+S(16.));
    }
    
    samp -= S(16.);
    
    if(0 < samp && samp < S(4.))
    {
        v += fermata1(samp);
        v += teller1(samp-S(2.)) * smoothstep(2.,4.,T(samp));
    }
    
    samp -= S(4.);
    
    if(0 < samp)
    {
        v += fullChorus(samp) * smoothstep(48.,32.,T(samp)); // fade out on chorus
    }
    
    return v;
}

vec2 mainSound( int samp, float _stt )
{
    vec2 v = vec2(0);
    float t = T(samp);
    v = fullSong(samp);
    
    //v = fullChorus(samp - 20., 10000.);
    //v = vec2(kick1(mod(samp, 0.5)));
    
    //v = fullDrop1(samp);
    
    //v = fermata1(samp);
    
    // Avoid clicks at the beginning
    return v * smoothstep(0.,0.01,t);
}
