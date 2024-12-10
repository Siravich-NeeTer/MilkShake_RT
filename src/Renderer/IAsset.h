#pragma once

#include <string>

namespace MilkShake
{
	class IAsset
	{
		public:
			IAsset(int _id, std::string _name)
				: m_ID(_id), m_Name(_name)
			{
			}

			int GetAssetID() const { return m_ID; }
			std::string GetAssetName() const { return m_Name; }

		protected:
			int m_ID;
			std::string m_Name;
	};
}