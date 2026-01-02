#pragma once

template <typename T>
class CUtlAutoList
{
public:
	virtual ~CUtlAutoList() = default;
	T* m_prev;
	T* m_next;
};