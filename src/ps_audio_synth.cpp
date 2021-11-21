

//calculate the frequency of the specified note.
//fractional notes allowed!
internal inline
f32 CalcFrequency(f32 fOctave,f32 fNote)
/*
	Calculate the frequency of any note!
	frequency = 440×(2^(n/12))
	N=0 is A4
	N=1 is A#4
	etc...
	notes go like so...
	0  = A
	1  = A#
	2  = B
	3  = C
	4  = C#
	5  = D
	6  = D#
	7  = E
	8  = F
	9  = F#
	10 = G
	11 = G#
*/
{
    double freq = 440*pow(2.0, fNote / 12.0); 
	return (float)(440*pow(2.0,((double)((fOctave-4)*12+fNote))/12.0));
}

internal inline f32
Oscilator_Sine(f32& phase, f32 frequency, f32 sampleRate)
{
    const f32 twoPi = 2.0f*Pi32;

    phase += twoPi*frequency/sampleRate;

	while(phase >= twoPi)
		phase -= twoPi;

	while(phase < 0)
		phase += twoPi;

    return sinf(phase);
}

internal inline f32
Oscilator_Square(f32& phase, f32 frequency, f32 sampleRate)
{
    phase += frequency/sampleRate;

	while(phase > 1.0f)
		phase -= 1.0f;

	while(phase < 0.0f)
		phase += 1.0f;

	if(phase <= 0.5f)
		return -1.0f;
	else
		return 1.0f;
}

internal inline
float Oscilator_Saw(float &fPhase, float fFrequency, float fSampleRate)
{
	fPhase += fFrequency/fSampleRate;

	while(fPhase > 1.0f)
		fPhase -= 1.0f;

	while(fPhase < 0.0f)
		fPhase += 1.0f;

	return (fPhase * 2.0f) - 1.0f;
}

internal inline
float Oscilator_Triangle(float &fPhase, float fFrequency, float fSampleRate)
{
	fPhase += fFrequency/fSampleRate;

	while(fPhase > 1.0f)
		fPhase -= 1.0f;

	while(fPhase < 0.0f)
		fPhase += 1.0f;

	float fRet;
	if(fPhase <= 0.5f)
		fRet=fPhase*2;
	else
		fRet=(1.0f - fPhase)*2;

	return (fRet * 2.0f) - 1.0f;
}

float AdvanceOscilator_Noise(float &fPhase, float fFrequency, float fSampleRate, float fLastValue)
{
	unsigned int nLastSeed = (unsigned int)fPhase;
	fPhase += fFrequency/fSampleRate;
	unsigned int nSeed = (unsigned int)fPhase;

	while(fPhase > 2.0f)
		fPhase -= 1.0f;

	if(nSeed != nLastSeed)
	{
		float fValue = ((float)rand()) / ((float)RAND_MAX);
		fValue = (fValue * 2.0f) - 1.0f;

		//uncomment the below to make it slightly more intense
		/*
		if(fValue < 0)
			fValue = -1.0f;
		else
			fValue = 1.0f;
		*/

		return fValue;
	}
	else
	{
		return fLastValue;
	}
}


float AdvanceOscilator_Saw_BandLimited(float &fPhase, float fFrequency, float fSampleRate, int nNumHarmonics /*=0*/)
{
	//advance the phase
	fPhase += 2 * (float)Pi32 * fFrequency/fSampleRate;

	while(fPhase >= 2 * (float)Pi32)
		fPhase -= 2 * (float)Pi32;

	while(fPhase < 0)
		fPhase += 2 * (float)Pi32;

	//if num harmonics is zero, calculate how many max harmonics we can do
	//without going over the nyquist frequency (half of sample rate frequency)
	if(nNumHarmonics == 0 && fFrequency != 0.0f)
	{
		float fTempFreq = fFrequency;

		while(fTempFreq < fSampleRate * 0.5f)
		{
			nNumHarmonics++;
			fTempFreq *= 2.0f;
		}
	}

	//calculate the saw wave sample
	float fRet = 0.0f;
	for(int nHarmonicIndex = 1; nHarmonicIndex <= nNumHarmonics; ++nHarmonicIndex)
	{
		fRet+=sinf(fPhase*(float)nHarmonicIndex)/(float)nHarmonicIndex;
	}

	//adjust the volume
	fRet = fRet * 2.0f / (float)Pi32;

	return fRet;
}

float AdvanceOscilator_Square_BandLimited(float &fPhase, float fFrequency, float fSampleRate, int nNumHarmonics/*=0*/)
{
	//advance the phase
	fPhase += 2 * (float)Pi32 * fFrequency/fSampleRate;

	while(fPhase >= 2 * (float)Pi32)
		fPhase -= 2 * (float)Pi32;

	while(fPhase < 0)
		fPhase += 2 * (float)Pi32;

	//if num harmonics is zero, calculate how many max harmonics we can do
	//without going over the nyquist frequency (half of sample rate frequency)
	if(nNumHarmonics == 0 && fFrequency != 0.0f)
	{
		while(fFrequency * (float)(nNumHarmonics*2-1) < fSampleRate * 0.5f)
		{
			nNumHarmonics++;
		}

		nNumHarmonics--;
	}

	//calculate the square wave sample
	float fRet = 0.0f;
	for(int nHarmonicIndex = 1; nHarmonicIndex <= nNumHarmonics; ++nHarmonicIndex)
	{
		fRet+=sinf(fPhase*(float)(nHarmonicIndex*2-1))/(float)(nHarmonicIndex*2-1);
	}

	//adjust the volume
	fRet = fRet * 4.0f / (float)Pi32;

	return fRet;
}

internal 
float AdvanceOscilator_Triangle_BandLimited(float &fPhase, float fFrequency, float fSampleRate, int nNumHarmonics/*=0*/)
{
	//advance the phase
	fPhase += 2 * (float)Pi32 * fFrequency/fSampleRate;

	while(fPhase >= 2 * (float)Pi32)
		fPhase -= 2 * (float)Pi32;

	while(fPhase < 0)
		fPhase += 2 * (float)Pi32;

	//if num harmonics is zero, calculate how many max harmonics we can do
	//without going over the nyquist frequency (half of sample rate frequency)
	if(nNumHarmonics == 0 && fFrequency != 0.0f)
	{
		while(fFrequency * (float)(nNumHarmonics*2-1) < fSampleRate * 0.5f)
		{
			nNumHarmonics++;
		}

		nNumHarmonics--;
	}

	//calculate the triangle wave sample
	bool bSubtract = true;

	float fRet = 0.0f;
	for(int nHarmonicIndex = 1; nHarmonicIndex <= nNumHarmonics; ++nHarmonicIndex)
	{
		if(bSubtract)
			fRet-=sinf(fPhase*(float)(nHarmonicIndex*2-1))/((float)(nHarmonicIndex*2-1) * (float)(nHarmonicIndex*2-1));
		else
			fRet+=sinf(fPhase*(float)(nHarmonicIndex*2-1))/((float)(nHarmonicIndex*2-1) * (float)(nHarmonicIndex*2-1));

		bSubtract = !bSubtract;
	}

	//adjust the volume
	fRet = fRet * 8.0f / ((float)Pi32 * (float)Pi32);

	return fRet;
}


internal inline
f32 Oscilator(f32& phase, f32 frequency, f32 sampleRate, f32 lastValue)
{
	return Oscilator_Sine(phase, frequency, sampleRate);
}