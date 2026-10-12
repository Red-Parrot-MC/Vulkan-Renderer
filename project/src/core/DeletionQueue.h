#pragma once
#include <deque>
#include <functional>

class DeletionQueue final
{
	std::deque<std::function<void()>> m_deletors;

public:
	void PushDeletor(std::function<void()>&& deletorFunc)
	{
		m_deletors.push_back(deletorFunc);
	}
	void Flush()
	{
		for (auto it = m_deletors.rbegin(); it != m_deletors.rend(); it++)
			(*it)();
		m_deletors.clear();
	}
};