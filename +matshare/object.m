classdef object
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
	
	
	properties (Dependent, SetAccess = private)
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
							obj(i,1) = matshare.object(shared_vars{i});
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
		
		function ret = abs(obj, varargin)
			if(nargout == 0)
				matshare_(8, 0, obj.shared_data, {}, varargin);
			else
				ret = matshare_(8, 0, obj.shared_data, {}, varargin);
			end
		end
		
		function ret = add(obj, in, varargin)
			if(nargout == 0)
				matshare_(8, 1, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 1, obj.shared_data, {in}, varargin);
			end
		end
		
		function ret = sub(obj, in, varargin)
			if(nargout == 0)
				matshare_(8, 2, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 2, obj.shared_data, {in}, varargin);
			end
		end
		
		function ret = mul(obj, in, varargin)
			if(nargout == 0)
				matshare_(8, 3, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 3, obj.shared_data, {in}, varargin);
			end
		end
		
		function ret = div(obj, in, varargin)
			if(nargout == 0)
				matshare_(8, 4, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 4, obj.shared_data, {in}, varargin);
			end
		end
		
		function ret = rem(obj, in, varargin)
			if(nargout == 0)
				matshare_(8, 5, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 5, obj.shared_data, {in}, varargin);
			end
		end
		
		function ret = mod(obj, in, varargin)
			if(nargout == 0)
				matshare_(8, 6, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 6, obj.shared_data, {in}, varargin);
			end
		end
		
		function ret = neg(obj, varargin)
			if(nargout == 0)
				matshare_(8, 7, obj.shared_data, {}, varargin);
			else
				ret = matshare_(8, 7, obj.shared_data, {}, varargin);
			end
		end
		
		function ret = ars(obj, in, varargin)
			if(nargout == 0)
				matshare_(8, 8, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 8, obj.shared_data, {in}, varargin);
			end
		end
		
		function ret = als(obj, in, varargin)
			if(nargout == 0)
				matshare_(8, 9, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 9, obj.shared_data, {in}, varargin);
			end
		end
		
		function ret = overwrite(obj, in, varargin)
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
			
			if(nargout == 0)
				matshare_(8, 10, obj.shared_data, {in}, varargin);
			else
				ret = matshare_(8, 10, obj.shared_data, {in}, varargin);
			end			
		end
		
		function clearshm(obj)
%% CLEAR  Clear the matshare object data from shared memory.
%    OBJ.CLEAR removes the data assocated to OBJ from shared memory.		
			
			matshare_(6, obj.shared_data);
			
		end
		
		function out = copy(obj)
%% COPY  Copy the matshare object data from shared memory.
%    OBJ.COPY copies the data assocated to OBJ from shared memory.
			
			out = matshare_(4, obj);
			
		end
		
		function out = testfind(obj)
			out = matshare_(13, obj.shared_data);
		end
		
		function obj = subsasgn(obj,S,B)
			
			switch(S(1).type(1))
				case '.'
					asgn_obj = obj;
				case '('
					asgn_obj = subsref(obj, S(1));
					S = S(2:end);
				otherwise
					error('matshare:object:IndexingError', ...
						 'Cannot index into matshare object with ''{}''');
			end
			
			if(~isscalar(asgn_obj))
				error('matshare:object:NonScalarObjectError', ...
					 'Assignment to shared memory requires a scalar matshare object');
			end
				
			if(S(1).type(1) == '.' && ischar(S(1).subs))
				if(strcmp(S(1).subs, 'data'))
					if(numel(S) < 2)
						error('matshare:object:DirectAssignmentError', ...
							 ['Direct assignment to the <strong>data</strong> ' ...
							  'property is not permitted. To assign all elements ' ...
							  'use the '':'' operator.']);
					end
					obj.overwrite(B, S(2:end));
				elseif(strcmp(S(1).subs, 'shared_data'))
					error('matshare:object:InaccessiblePropertyError', 'The shared_data property is not directly accessible.');
				else
					error('matshare:object:InvalidPropertyError', ['The property.''' S(1).subs ''' does not exist.']);
				end
			else
				error('matshare:object:InvalidAccessError', ['Could not set the given property or index. Access shared memory with the '...
				'<strong>data</strong> property.']);
			end
		end
		
		function disp(obj)
			if(isscalar(obj))
				dispstr = evalc('builtin(''disp'',obj);');
				dispstr = dispstr(strfind(dispstr, 'data:'):end-1);
				
				if(issparse(obj.data))
					fprintf([ ...
					'  <a href="matlab:helpPopup matshare.object" ' ...
					'style="font-weight:bold">matshare object</a> storing ' ...
					'<strong>%s</strong> ' ...
					'<a href="matlab:helpPopup sparse" style="font-weight:bold">sparse</a> ' ...
					'<a href="matlab:helpPopup %s" style="font-weight:bold">%s</a>' ...
					':\n    %s\n'], ...
					regexprep(num2str(size(obj.data)), '\s+', 'x'), ...
					class(obj.data), class(obj.data), ...
					dispstr);
				else
					fprintf([ ...
					'  <a href="matlab:helpPopup matshare.object" ' ...
					'style="font-weight:bold">matshare object</a> storing ' ...
					'<strong>%s</strong> ' ...
					'<a href="matlab:helpPopup %s" style="font-weight:bold">%s</a>' ...
					':\n    %s\n'], ...
					regexprep(num2str(size(obj.data)), '\s+', 'x'), ...
					class(obj.data), class(obj.data), ...
					dispstr);
				end
			
			elseif(isempty(obj))
				disp(['  <strong>empty</strong> <a href="matlab:helpPopup matshare.object" ' ...
				'style="font-weight:bold">matshare object</a>' ...
				' array']);
			else
				disp(['  <strong>' regexprep(num2str(size(obj)), '\s+', 'x') ...
					'</strong> <a href="matlab:helpPopup matshare.object" ' ...
				'style="font-weight:bold">matshare object</a>' ...
				' array']);
			end
		end
		
	end
end
