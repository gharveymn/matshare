classdef matshare
	
	properties (SetAccess = immutable)
		data
	end
	
	methods
		function obj = matshare(in, noshare)
			% normal assignment so we don't share the variable twice
			if(nargin > 1 && noshare)
				obj.data = in;
			elseif(nargin > 0)
				obj.data = matshare_(0, in);
			end
		end
		
		function cleardata(obj)
			matshare_(6, obj.data);
		end
		
		function ret = makelocalcopy(obj)
			ret = matshare_(4, obj.data);
		end
				
	end
	
	methods (Static)
		
		% these are just the entry functions but in static method form
		
		function newobj = share(in)
			switch(nargout)
				case 0
					matshare_(0, in);
				case 1
					newobj = matshare(in);
			end
		end
		
		function [newestobj, newvarobjs, allvarobjs] = fetch
			if(nargout <= 1)
				newestobj = matshare(matshare_(1), true);
			elseif(nargout == 3)
				[newestobj, newvarobjs, allvarobjs] = matshare_(1);
				
				newestobj = matshare(newestobj, true);
				
				for i = 1:numel(newvarobjs)
					newvarobjs{i} = matshare(newvarobjs{i}, true);
				end
				
				for i = 1:numel(allvarobjs)
					allvarobjs{i} = matshare(allvarobjs{i}, true);
				end
				
			else
				[newestobj, newvarobjs] = matshare_(1);
				
				newestobj = matshare(newestobj, true);
				
				for i = 1:numel(newvarobjs)
					newvarobjs{i} = matshare(newvarobjs{i}, true);
				end
			end
		end
		
		function detach
			matshare_(2);
		end
		
		function config(varargin)
		    matshare_(3,varargin{:});
		end
		
		function ret = localcopy(in)
			ret = matshare_(4, in);
		end
		
		function debug
			matshare_(5)
		end
		
		function clear(varargin)
			matshare_(6, varargin{:});
		end
		
		function reset
			matshare_(7)
		end
		
	end
end

