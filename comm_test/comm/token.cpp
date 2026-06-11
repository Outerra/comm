#include <comm/token.h>
static const coid::token src = "foo//bar//baz";
static const coid::token src_group = "foo/\\bar/\\baz";


void test_cuts_default_empty_and_full() 
{
    coid::token test = src;
    coid::token cut;

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    // substr
    {
        // default empty
        {
            // keep sep
            {
                cut = test.cut_left("///"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_left_back("///"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right("///"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_back("///"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                DASSERT(test == src && cut.is_empty());
            }

            // return sep
            {
                cut = test.cut_left("///"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_left_back("///"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right("///"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_back("///"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                DASSERT(test == src && cut.is_empty());
            }

            // remove sep
            {
                cut = test.cut_left("///"_T, coid::token::cut_trait_remove_sep_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_left_back("///"_T, coid::token::cut_trait_remove_sep_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right("///"_T, coid::token::cut_trait_remove_sep_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_back("///"_T, coid::token::cut_trait_remove_sep_default_empty());
                DASSERT(test == src && cut.is_empty());
            }
        }

        // default full
        {
            // keep sep
            {
                cut = test.cut_left("///"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_left_back("///"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right("///"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_back("///"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;
            }

            // return sep
            {
                cut = test.cut_left("///"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_left_back("///"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right("///"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_back("///"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;
            }

            // remove sep
            {
                cut = test.cut_left("///"_T, coid::token::cut_trait_remove_sep_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_left_back("///"_T, coid::token::cut_trait_remove_sep_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right("///"_T, coid::token::cut_trait_remove_sep_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_back("///"_T, coid::token::cut_trait_remove_sep_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////
    // group
    {
        // default empty
        {
            // keep sep
            {
                cut = test.cut_left_group("*"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_left_group_back("*"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_group("*"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_group_back("*"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                DASSERT(test == src && cut.is_empty());
            }

            // return sep
            {
                cut = test.cut_left_group("*"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_left_group_back("*"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_group("*"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_group_back("*"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                DASSERT(test == src && cut.is_empty());
            }

            // remove sep
            {
                cut = test.cut_left_group("*"_T, coid::token::cut_trait_remove_sep_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_left_group_back("*"_T, coid::token::cut_trait_remove_sep_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_group("*"_T, coid::token::cut_trait_remove_sep_default_empty());
                DASSERT(test == src && cut.is_empty());

                cut = test.cut_right_group_back("*"_T, coid::token::cut_trait_remove_sep_default_empty());
                DASSERT(test == src && cut.is_empty());
            }
        }

        // default full
        {
            // keep sep
            {
                cut = test.cut_left_group("*"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_left_group_back("*"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_group("*"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_group_back("*"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;
            }

            // return sep
            {
                cut = test.cut_left_group("*"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_left_group_back("*"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_group("*"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_group_back("*"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;
            }

            // remove sep
            {
                cut = test.cut_left_group("*"_T, coid::token::cut_trait_remove_sep_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_left_group_back("*"_T, coid::token::cut_trait_remove_sep_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_group("*"_T, coid::token::cut_trait_remove_sep_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;

                cut = test.cut_right_group_back("*"_T, coid::token::cut_trait_remove_sep_default_full());
                DASSERT(cut == src && test.is_empty());
                test = src;
            }
        }
    }
}

void test_cuts_valid_substr()
{
    coid::token test;
    coid::token cut;

    //simple separator
    {
        // left
        {
            // front
            {
                // keep sep 
                {
                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(test == "//bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(test == "/bar//baz"_T && cut == "foo/"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(test == "//bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(test == "/bar//baz"_T && cut == "foo/"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(test == "//bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(test == "/bar//baz"_T && cut == "foo/"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(test == "//bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(test == "/bar//baz"_T && cut == "foo/"_T);
                }

                // remove sep
                {
                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(test == "/bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(test == "/bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(test == "/bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("/"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(test == "/bar//baz"_T && cut == "foo"_T);
                }
            }

            // back
            {
                // keep sep 
                {
                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(test == "/baz"_T && cut == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(test == "baz"_T && cut == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(test == "/baz"_T && cut == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(test == "baz"_T && cut == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(test == "/baz"_T && cut == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(test == "/baz"_T && cut == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo//bar//"_T);
                }

                // remove sep
                {
                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(test == "baz"_T && cut == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(test == "baz"_T && cut == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_left_back("/"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo//bar/"_T);
                }
            }
        }

        // right
        {
            // front
            {
                // keep sep 
                {
                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(cut == "/bar//baz"_T && test == "foo/"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(cut == "//bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(cut == "/bar//baz"_T && test == "foo/"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(cut == "//bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(cut == "/bar//baz"_T && test == "foo/"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(cut == "//bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(cut == "/bar//baz"_T && test == "foo/"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(cut == "//bar//baz"_T && test == "foo"_T);
                }

                // remove sep
                {
                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(cut == "/bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(cut == "/bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(cut == "/bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("/"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(cut == "/bar//baz"_T && test == "foo"_T);
                }
            }

            // back
            {
                // keep sep 
                {
                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(cut == "baz"_T && test == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(cut == "/baz"_T && test == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(cut == "baz"_T && test == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(cut == "/baz"_T && test == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(cut == "/baz"_T && test == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(cut == "/baz"_T && test == "foo//bar/"_T);
                }

                // remove sep
                {
                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(cut == "baz"_T && test == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(cut == "baz"_T && test == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo//bar/"_T);

                    test = src;
                    cut = test.cut_right_back("/"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo//bar/"_T);
                }
            }
        }
    }

    //double separator
    {
        // left
        {
            // front
            {
                // keep sep 
                {
                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(test == "//bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(test == "bar//baz"_T && cut == "foo//"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(test == "//bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(test == "bar//baz"_T && cut == "foo//"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(test == "//bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(test == "bar//baz"_T && cut == "foo//"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(test == "//bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(test == "bar//baz"_T && cut == "foo//"_T);
                }

                // remove sep
                {
                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(test == "bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(test == "bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(test == "bar//baz"_T && cut == "foo"_T);

                    test = src;
                    cut = test.cut_left("//"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(test == "bar//baz"_T && cut == "foo"_T);
                }
            }

            // back
            {
                // keep sep 
                {
                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(test == "//baz"_T && cut == "foo//bar"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(test == "baz"_T && cut == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(test == "//baz"_T && cut == "foo//bar"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(test == "baz"_T && cut == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(test == "//baz"_T && cut == "foo//bar"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(test == "//baz"_T && cut == "foo//bar"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo//bar//"_T);
                }

                // remove sep
                {
                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(test == "baz"_T && cut == "foo//bar"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(test == "baz"_T && cut == "foo//bar"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo//bar"_T);

                    test = src;
                    cut = test.cut_left_back("//"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo//bar"_T);
                }
            }
        }

        // right
        {
            // front
            {
                // keep sep 
                {
                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(cut == "bar//baz"_T && test == "foo//"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(cut == "//bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(cut == "bar//baz"_T && test == "foo//"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(cut == "//bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(cut == "bar//baz"_T && test == "foo//"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(cut == "//bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(cut == "bar//baz"_T && test == "foo//"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(cut == "//bar//baz"_T && test == "foo"_T);
                }

                // remove sep
                {
                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(cut == "bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(cut == "bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(cut == "bar//baz"_T && test == "foo"_T);

                    test = src;
                    cut = test.cut_right("//"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(cut == "bar//baz"_T && test == "foo"_T);
                }
            }

            // back
            {
                // keep sep 
                {
                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(cut == "baz"_T && test == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(cut == "//baz"_T && test == "foo//bar"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(cut == "baz"_T && test == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(cut == "//baz"_T && test == "foo//bar"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(cut == "//baz"_T && test == "foo//bar"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo//bar//"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(cut == "//baz"_T && test == "foo//bar"_T);
                }

                // remove sep
                {
                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(cut == "baz"_T && test == "foo//bar"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(cut == "baz"_T && test == "foo//bar"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo//bar"_T);

                    test = src;
                    cut = test.cut_right_back("//"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo//bar"_T);
                }
            }
        }
    }
}

void test_cuts_valid_group()
{

    coid::token test;
    coid::token cut;

    //simple separator
    {
        // left
        {
            // front
            {
                // keep sep 
                {
                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(test == "/\\bar/\\baz"_T && cut == "foo"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(test == "\\bar/\\baz"_T && cut == "foo/"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(test == "/\\bar/\\baz"_T && cut == "foo"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(test == "bar/\\baz"_T && cut == "foo/\\"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(test == "/\\bar/\\baz"_T && cut == "foo"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(test == "\\bar/\\baz"_T && cut == "foo/"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(test == "/\\bar/\\baz"_T && cut == "foo"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(test == "bar/\\baz"_T && cut == "foo/\\"_T);
                }

                // remove sep
                {
                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(test == "\\bar/\\baz"_T && cut == "foo"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(test == "bar/\\baz"_T && cut == "foo"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(test == "\\bar/\\baz"_T && cut == "foo"_T);

                    test = src_group;
                    cut = test.cut_left_group("*/-\\+"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(test == "bar/\\baz"_T && cut == "foo"_T);
                }
            }

            // back
            {
                // keep sep 
                {
                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(test == "\\baz"_T && cut == "foo/\\bar/"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(test == "baz"_T && cut == "foo/\\bar/\\"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(test == "/\\baz"_T && cut == "foo/\\bar"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(test == "baz"_T && cut == "foo/\\bar/\\"_T);
                    
                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(test == "\\baz"_T && cut == "foo/\\bar/"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo/\\bar/\\"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(test == "/\\baz"_T && cut == "foo/\\bar"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo/\\bar/\\"_T);
                }

                // remove sep
                {
                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(test == "baz"_T && cut == "foo/\\bar/"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(test == "baz"_T && cut == "foo/\\bar"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo/\\bar/"_T);

                    test = src_group;
                    cut = test.cut_left_group_back("*/-\\+"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(test == "baz"_T && cut == "foo/\\bar"_T);
                }
            }
        }

        // right
        {
            // front
            {
                // keep sep 
                {
                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(cut == "\\bar/\\baz"_T && test == "foo/"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(cut == "/\\bar/\\baz"_T && test == "foo"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(cut == "bar/\\baz"_T && test == "foo/\\"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(cut == "/\\bar/\\baz"_T && test == "foo"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(cut == "\\bar/\\baz"_T && test == "foo/"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(cut == "/\\bar/\\baz"_T && test == "foo"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(cut == "bar/\\baz"_T && test == "foo/\\"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(cut == "/\\bar/\\baz"_T && test == "foo"_T);
                }

                // remove sep
                {
                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(cut == "\\bar/\\baz"_T && test == "foo"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(cut == "bar/\\baz"_T && test == "foo"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(cut == "\\bar/\\baz"_T && test == "foo"_T);

                    test = src_group;
                    cut = test.cut_right_group("*/-\\+"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(cut == "bar/\\baz"_T && test == "foo"_T);
                }
            }

            // back
            {
                // keep sep 
                {
                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_source_default_full());
                    DASSERT(cut == "baz"_T && test == "foo/\\bar/\\"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_returned_default_full());
                    DASSERT(cut == "\\baz"_T && test == "foo/\\bar/"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_source_default_full());
                    DASSERT(cut == "baz"_T && test == "foo/\\bar/\\"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_full());
                    DASSERT(cut == "/\\baz"_T && test == "foo/\\bar"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_source_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo/\\bar/\\"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_with_returned_default_empty());
                    DASSERT(cut == "\\baz"_T && test == "foo/\\bar/"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_source_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo/\\bar/\\"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_keep_sep_all_with_returned_default_empty());
                    DASSERT(cut == "/\\baz"_T && test == "foo/\\bar"_T);
                }

                // remove sep
                {
                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_remove_sep_default_full());
                    DASSERT(cut == "baz"_T && test == "foo/\\bar/"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_remove_sep_all_default_full());
                    DASSERT(cut == "baz"_T && test == "foo/\\bar"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_remove_sep_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo/\\bar/"_T);

                    test = src_group;
                    cut = test.cut_right_group_back("*/-\\+"_T, coid::token::cut_trait_remove_sep_all_default_empty());
                    DASSERT(cut == "baz"_T && test == "foo/\\bar"_T);
                }
            }
        }
    }
}

void test_token_cuts()
{
    test_cuts_default_empty_and_full();
    test_cuts_valid_substr();
    test_cuts_valid_group();
}

void run_token_tests() 
{
    test_token_cuts();
}