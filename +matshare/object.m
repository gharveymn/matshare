classdef object < matlab.mixin.CustomDisplay
%% MATSHARE.OBJECT stores the shared memory as an object
%    This class is designed to store the shared data inside a 
%    cell array so that shared data pointers are maintained. To
%    access the data simply create an instance and query the <a href="matlab:help matshare.object/data">data</a>
%    property.
%
%    Methods:
%        matshare.object.overwrite    - Overwrite the variable in-place
%        matshare.object.copy         - Copy the variable from shared memory
%        matshare.object.clear        - Clear the variable from shared memory
%
%    Instances of this class should only be created via <a href="matlab:help matshare.share">matshare.share</a>,
%    <a href="matlab:help matshare.pshare">matshare.pshare</a>, and <a href="matlab:help matshare.fetch">matshare.fetch</a>.

%% Copyright © 2018 Gene Harvey
%    This software may be modified and distributed under the terms
%    of the MIT license. See the LICENSE file for details.
	
	properties (Hidden, Access = protected)
		shared_data = {[]}; % Shares shared data pointers in a cell.
	end
	
	
	properties (Dependent, SetAccess = immutable)
		data % Query this property to access shared memory.
	end
	
	methods
		
		function obj = object(shared_vars, num_objs)
%% OBJECT  Create matshare objects.
%    OBJ = OBJECT Creates a scalar matshare object.
%
%    OBJ = OBJECT(S) Creates a scalar matshare object storing S as data.
%
%    OBJ = OBJECT(S,N) Creates an array of matshare objects interpreting S
%    as a cell array containing data to be stored with N elements.
%
%    Note: Do not call this directly. Use <a href="matlab:help matshare.share">matshare.share</a>,
%    <a href="matlab:help matshare.pshare">matshare.pshare</a>, and <a href="matlab:help
%    matshare.fetch">matshare.fetch</a> to create and fetch variables.
			
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
%    OBJ = OBJ.OVERWRITE(IN) recursively overwrites the 
%    contents of the matshare object OBJ with IN. The data stored by OBJ 
%    must have exactly the same size as IN.
%
%    OBJ = OBJ.OVERWRITE(IN,OP) does the same except one may specify if 
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
%    OBJ.CLEAR removes the data assocated to OBJ from shared memory.		
			
			matshare_(6, obj.shared_data);
			
		end
		
		function out = copy(obj)
%% COPY  Copy the matshare object data from shared memory.
%    OBJ.COPY copies the data assocated to OBJ from shared memory.
			
			out = matshare_(4, obj);
			
		end
		
	end
	
	methods (Hidden, Access = protected)
		function header = getHeader(obj)
			if(isscalar(obj))
								
				header = ['<a href="matlab:helpPopup matshare.object" ' ...
				'style="font-weight:bold">matshare object</a>' ...
				' storing '];
				if(iscell(obj.data))
					header = [header ...
							'<a href="matlab:helpPopup cell" ' ...
							'style="font-weight:bold">cell array</a>'];
				elseif(ischar(obj.data))
					header = [header ...
							'<a href="matlab:helpPopup char" ' ...
							'style="font-weight:bold">character array</a>'];
				else
					if(isscalar(obj.data))
						header = [header 'scalar '];
					elseif(isempty(obj.data))
						header = [header 'empty '];
					end
					header = [header ...
								'<a href="matlab:helpPopup ' class(obj.data) '" ' ...
								'style="font-weight:bold">' class(obj.data) '</a>'];
					if(~isscalar(obj.data))
						header = [header ' array'];
					end
				end
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
		
		function groups = getPropertyGroups(obj)
			groups = getPropertyGroups@matlab.mixin.CustomDisplay(obj);
		end
		
		function s = getFooter(obj)
			s = getFooter@matlab.mixin.CustomDisplay(obj);
		end
		
		function displayScalarObject(obj)
			displayScalarObject@matlab.mixin.CustomDisplay(obj);
		end
		
		function displayNonScalarObject(obj)
			disp(getHeader(obj));
		end
		
		function displayEmptyObject(obj)
			disp(getHeader(obj));
		end
		
		function displayScalarHandleToDeletedObject(obj)
			displayScalarHandleToDeletedObject@matlab.mixin.CustomDisplay(obj);
		end
		
		
	end
end