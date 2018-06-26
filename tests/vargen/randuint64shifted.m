function shifted = randuint64shifted(rns, dims)
	%INT64SHIFT Summary of this function goes here
	%   Detailed explanation goes here
	shifted = uint64(randi(rns, intmax('uint32'), dims, 'uint32'));
end

