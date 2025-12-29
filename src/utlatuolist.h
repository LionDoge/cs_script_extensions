#pragma once

template <typename T>
class CUtlAutoList
{
public:
	T* m_prev;
	T* m_next;
};