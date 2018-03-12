classdef MatShare < handle

	properties (Transient, Dependent, GetObservable, SetObservable)
		data
	end

	properties (Constant, Access=private)
		SHARE      = uint8(0);
          FETCH      = uint8(1);
		DETACH     = uint8(2);
          PARAM      = uint8(3);
		DEEPCOPY   = uint8(4);
		DEBUG      = uint8(5);
		REGISTER   = uint8(6);
		DEREGISTER = uint8(7);
		INIT       = uint8(8);
	end

	methods
		function obj = MatShare
			matshare_(MatShare.REGISTER);
		end

		function set.data(~,in)
			matshare_(MatShare.SHARE, in);
		end

		function out = get.data(~)
 			out = matshare_(MatShare.FETCH);
		end

		function ret = deepcopy(~)
			ret = matshare_(MatShare.DEEPCOPY);
		end

		function debug(~)
			matshare_(MatShare.DEBUG);
		end

		function delete(~)
			matshare_(MatShare.DEREGISTER);
			clear matshare_
		end

	end

	methods (Static)

		function share(in)
			matshare_(MatShare.SHARE, in);
		end

		function out = fetch()
			% get callback
			out = matshare_(MatShare.FETCH);
        end

        function detach()
           matshare_(MatShare.DETACH);
        end

	end
end