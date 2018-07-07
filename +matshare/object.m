classdef object < matlab.mixin.CustomDisplay
%% MATSHARE.OBJECT stores the shared memory as an object
%    This class is designed to store the shared data inside a 
%    cell array so that shared data pointers are maintained. To
%    access the data simply create an instance and query the DATA
%    property.
%
%    Instances of class may only be created via MATSHARE.SHARE,
%    MATSHARE.PSHARE, and MATSHARE.FETCH.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	properties (Hidden, Access = protected)
		shared_data = {[]}; % stored data pointers
	end
	
	
	properties (Dependent, SetAccess = immutable)
		data % property that users will query
	end
	
	methods
		
		function obj = object(shared_vars, num_objs)
			if(nargin > 0)
				if(nargin > 1)
					% if object array is specified use as a cell array
					if(num_objs == 0)
						obj = matshare.object.empty;
					else
						for i = num_objs:-1:1
							obj(i) = matshare.object(shared_vars{i});
						end
					end
				else
					% else set as data
					obj.shared_data = shared_vars;
				end
			end
		end
		
		function ret = get.data(obj)
			ret = obj.shared_data{1};
		end
		
		function obj = overwrite(obj, in, option)
%% OVERWRITE  Overwrite the contents of a variable in-place.
%    MOOUT = MO.OVERWRITE(IN) recursively overwrites the 
%    contents of the matshare object MO with IN. The data stored by MO 
%    must have exactly the same size as IN.
%
%    MOOUT = MO.OVERWRITE(IN,OP) does the same except one may specify if 
%    the operation is to be done in a synchronous manner. To run 
%    synchronously use the flag '-s', otherwise use the flag '-a'.
%
%    This function is asynchronous by default.
			
			if(nargin == 3)
				matshare_(8, obj.shared_data, in, option);
			else
				matshare_(8, obj.shared_data, in);
			end
			
		end
		
		function clear(obj)
%% CLEAR  Clear the matshare object data from shared memory.
%    MO.CLEAR removes the data assocated to MO from shared memory.		
			
			matshare_(6, obj.shared_data);
			
		end
		
		function out = copy(obj)
%% COPY  Copy the matshare object data from shared memory.
%    MO.COPY copies the data assocated to MO from shared memory.
			
			out = matshare_(4, obj);
			
		end
		
	end
	
	methods (Access = protected)	
		function header = getHeader(obj)
			if(isscalar(obj))
				header = ['<a href="matlab:helpPopup matshare.object" ' ...
				'style="font-weight:bold">matshare object</a>' ...
				' storing ' ...
				'<a href="matlab:helpPopup ' class(obj.data) '" ' ...
				'style="font-weight:bold">' class(obj.data) '</a>'];
			elseif(isempty(obj))
				header = ['empty <a href="matlab:helpPopup matshare.object" ' ...
				'style="font-weight:bold">matshare object</a>' ...
				' array'];
			else
				header = [matlab.mixin.CustomDisplay.convertDimensionsToString(obj) ...
					' <a href="matlab:helpPopup matshare.object" ' ...
				'style="font-weight:bold">matshare object</a>' ...
				' array'];
			end
			
			% This is not a handle class, so no handle case
			
		end
		
		function displayNonScalarObject(obj)
			disp(getHeader(obj));
		end
		
		function displayEmptyObject(obj)
			disp(getHeader(obj));
		end
		
	end
end