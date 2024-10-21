#include "imageanalysis.h"

double NormalizeValue(double fValue, double fOrigRange, double fMinOrig, double fNewRange, double fMinNew)
{
    return fOrigRange ? ((fValue - fMinOrig) / fOrigRange) * fNewRange + fMinNew : fMinNew;
}

void UpdatePrintAnalysisOpts(ImageAnalysis* pImageAnalysis, AnalysisOpts* pOpts)
{
    if (pOpts)
    {
        pImageAnalysis->opts = *pOpts;
    }
}
