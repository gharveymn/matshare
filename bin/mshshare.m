function shared_variable = mshshare(in)
	if(nargout == 0)
		matshare_(0, in);
	else
		shared_variable = matshare_(0, in);
		matshare_(12);
	end
end

