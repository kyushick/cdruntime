SUBDIRS = \
    test_simple_hierarchy \
    test_user_defined_hierarhcy \
    test_serdes \
		test_tuning \
		test_vector \
		libc_logging

if HAVE_MPI
    SUBDIRS += test_prv_ref_remote
endif
clean-tests:
	cd test_simple_hierarchy; make clean; \
	cd test_user_defined_hierarhcy; make clean; \
	cd test_prv_ref_remote; make clean;
	cd test_serdes; make clean;
	cd test_tuning; make clean;
	cd test_vector; make clean;
	cd libc_logging; make clean;
