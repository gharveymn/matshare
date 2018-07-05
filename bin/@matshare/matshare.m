classdef matshare < handle & matlab.mixin.CustomDisplay
	
	% This is stored information
	properties (Hidden, Access = protected)
		shared_data;
	end
	
	% This is the property that users will query
	properties (Dependent, SetAccess = immutable)
		data
	end
	
	methods
		function ret = get.data(obj)
			ret = obj.shared_data{1};
		end
		
		function obj = overwrite(obj, in, option)
		%% OVERWRITE  Overwrite the contents of a variable in-place.
		%    OVERWRITE(DEST, INPUT) recursively overwrites the contents of DEST 
		%    with the contents of INPUT. The two input variables must have exactly 
		%    the same size.
			
			if(nargin == 3)
				matshare_(8, obj.shared_data, in, option);
			else
				matshare_(8, obj.shared_data, in);
			end
		end
	end
	
	methods (Access = protected)
		function obj = matshare(shared_vars, num_objs)
			if(nargin > 0)
				if(nargin > 1)
					% if object array is specified use as a cell array
					for i = num_objs:-1:1
						obj(i) = matshare(shared_vars{i});
					end
				else
					% else set as data
					obj.shared_data = shared_vars;
				end
			end
		end
		
		function header = getHeader(obj)
			if(isscalar(obj))
				if(iscell(obj.shared_data))
					header = ['<a href="matlab:helpPopup matshare" ' ...
					'style="font-weight:bold">matshare object</a>' ...
					' storing ' ...
					'<a href="matlab:helpPopup ' class(obj.data) '" ' ...
					'style="font-weight:bold">' class(obj.data) '</a>'];
				else
					header = ['empty ' ...
					'<a href="matlab:helpPopup matshare" ' ...
					'style="font-weight:bold">matshare object</a>' ...
					];
				end
			else
				header = getHeader@matlab.mixin.CustomDisplay(obj);
			end
		end
	end
	
	methods (Static)
		
		% share
		varargout = share(varargin)
		
		% fetch
		fetched = fetch(varargin)
		
		% clear
		clear(varargin)
		
		% pshare
		pshare
		
	end
end

