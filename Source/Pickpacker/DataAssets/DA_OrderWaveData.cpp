#include "DataAssets/DA_OrderWaveData.h"

const FParcelOrderWave* UDA_OrderWaveData::GetWave(int32 WaveIndex) const
{
	if (OrderWaves.IsValidIndex(WaveIndex))
	{
		return &OrderWaves[WaveIndex];
	}
	return nullptr;
}


