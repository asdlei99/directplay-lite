/* DirectPlay Lite
 * Copyright (C) 2018 Daniel Collins
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef DPLITE_COMAPIEXCEPTION_HPP
#define DPLITE_COMAPIEXCEPTION_HPP

#include <winsock2.h>
#include <exception>
#include <windows.h>

class COMAPIException: public std::exception
{
	private:
		const HRESULT hr;
		char what_s[64];
		
	public:
		COMAPIException(HRESULT result);
		virtual ~COMAPIException();
		
		HRESULT result() const noexcept;
		virtual const char *what() const noexcept;
};

#endif /* !DPLITE_COMAPIEXCEPTION_HPP */
