break ILLOP

define pq
	set gdb_print_q ($)
	printf "\n"
end

define pq1
	set gdb_print_q (vmread ($arg0))
	printf "\n"
end

define pa
	set gdb_print_q (vmread ($arg0))
	printf "\n"
end

define pv
	set gdb_print_q_verbose ($)
	printf "\n"
end

define pv1
	set gdb_print_q_verbose (vmread ($arg0))
	printf "\n"
end

define pav
	set gdb_print_q_verbose (vmread ($arg0))
	printf "\n"
end

define ppdl
	p/o vm[(pdl_data_base + $arg0) & 0xffffff]
	pq
end

define pvm
	p/o vm[$arg0 & 0xffffff]
	pq
end
