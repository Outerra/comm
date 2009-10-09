#include "../token.h"
#include "../local.h"
#include "../commexception.h"

#include "../regex.h"
#include "regcomp.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
Reinst* regex_compiler::create( Reinst::OP type ) {
    Reinst* r = new Reinst(type);
    *_prog->rinst.add() = r;
    return r;
}

////////////////////////////////////////////////////////////////////////////////
void regex_compiler::operand(Reinst::OP t)
{
    if(_lastwasand)
        xoperator(Reinst::CAT);	// catenate is implicit

    Reinst* i = create(t);

    if(t == Reinst::CCLASS || t == Reinst::NCCLASS)
        i->cp = _yyclassp;
    if(t == Reinst::RUNE)
        i->cd = _yyrune;

    pushand(i, i);
    _lastwasand = 1;
}

////////////////////////////////////////////////////////////////////////////////
void regex_compiler::xoperator(Reinst::OP t)
{
    if(t==Reinst::RBRA && --_nopenbraces<0)
        throw exception() << "unmatched right paren";
    if(t==Reinst::LBRA) {
        ++_cursubid;
        ++_nopenbraces;
        if(_lastwasand)
            xoperator(Reinst::CAT);
    }
    else
        evaluntil(t);

    if(t != Reinst::RBRA)
        pushator(t);

    _lastwasand = 0;
    
    if(t==Reinst::STAR || t==Reinst::QUEST || t==Reinst::PLUS || t==Reinst::RBRA)
        _lastwasand = 1;	// these look like operands
}

////////////////////////////////////////////////////////////////////////////////
void regex_compiler::evaluntil(Reinst::OP pri)
{
    Node *op1, *op2;
    Reinst *inst1, *inst2;

    Ator* a;
    while((a=_atorstack.last())->ator >= pri || pri==Reinst::RBRA){
        switch(popator()){
        default:
            throw exception() << "unknown operator in evaluntil";
            break;
        case Reinst::LBRA:		// must have been RBRA
            op1 = popand('(');
            inst2 = create(Reinst::RBRA);
            inst2->subid = a->subid;
            op1->last->next = inst2;
            inst1 = create(Reinst::LBRA);
            inst1->subid = a->subid;
            inst1->next = op1->first;
            pushand(inst1, inst2);
            return;
        case Reinst::OR:
            op2 = popand('|');
            op1 = popand('|');
            inst2 = create(Reinst::NOP);
            op2->last->next = inst2;
            op1->last->next = inst2;
            inst1 = create(Reinst::OR);
            inst1->right = op1->first;
            inst1->left = op2->first;
            pushand(inst1, inst2);
            break;
        case Reinst::CAT:
            op2 = popand(0);
            op1 = popand(0);
            op1->last->next = op2->first;
            pushand(op1->first, op2->last);
            break;
        case Reinst::STAR:
            op2 = popand('*');
            inst1 = create(Reinst::OR);
            op2->last->next = inst1;
            inst1->right = op2->first;
            pushand(inst1, inst1);
            break;
        case Reinst::PLUS:
            op2 = popand('+');
            inst1 = create(Reinst::OR);
            op2->last->next = inst1;
            inst1->right = op2->first;
            pushand(op2->first, inst1);
            break;
        case Reinst::QUEST:
            op2 = popand('?');
            inst1 = create(Reinst::OR);
            inst2 = create(Reinst::NOP);
            inst1->left = inst2;
            inst1->right = op2->first;
            op2->last->next = inst2;
            pushand(inst1, inst2);
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
void regex_program::optimize()
{
    // get rid of NOOP chains
    Reinst** ib = rinst.ptr();
    Reinst** ie = rinst.ptre() - 1; //omit END
    for( ; ib<ie; ++ib ){
        Reinst* target = (*ib)->next;
        while(target->type == Reinst::NOP)
            target = target->next;
        (*ib)->next = target;
    }
}

////////////////////////////////////////////////////////////////////////////////
#ifdef	DEBUG
static	void
dumpstack(void){
    Node *stk;
    int *ip;

    print("operators\n");
    for(ip=atorstack; ip<atorp; ip++)
        print("0%o\n", *ip);
    print("operands\n");
    for(stk=andstack; stk<andp; stk++)
        print("0%o\t0%o\n", stk->first->type, stk->last->type);
}

static	void
dump(Reprog *pp)
{
    Reinst *l;
    ucs4 *p;

    l = pp->firstinst;
    do{
        print("%d:\t0%o\t%d\t%d", l-pp->firstinst, l->type,
            l->left-pp->firstinst, l->right-pp->firstinst);
        if(l->type == RUNE)
            print("\t%C\n", l->r);
        else if(l->type == CCLASS || l->type == NCCLASS){
            print("\t[");
            if(l->type == NCCLASS)
                print("^");
            for(p = l->cp->spans; p < l->cp->end; p += 2)
                if(p[0] == p[1])
                    print("%C", p[0]);
                else
                    print("%C-%C", p[0], p[1]);
            print("]\n");
        } else
            print("\n");
    }while(l++->type);
}
#endif

////////////////////////////////////////////////////////////////////////////////
int regex_compiler::nextc(ucs4 *rp)
{
    if(_string.is_empty()) {
        *rp = 0;
        return 0;
    }

    *rp = _string.cut_utf8();
    if(*rp == L'\\') {
        *rp = _string.cut_utf8();
        return 1;
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
Reinst::OP regex_compiler::lex(int literal, Reinst::OP dot_type)
{
    int quoted;

    quoted = nextc(&_yyrune);
    if(literal || quoted){
        if(_yyrune == 0)
            return Reinst::END;
        return Reinst::RUNE;
    }

    switch(_yyrune){
    case 0:
        return Reinst::END;
    case L'*':
        return Reinst::STAR;
    case L'?':
        return Reinst::QUEST;
    case L'+':
        return Reinst::PLUS;
    case L'|':
        return Reinst::OR;
    case L'.':
        return dot_type;
    case L'(':
        return Reinst::LBRA;
    case L')':
        return Reinst::RBRA;
    case L'^':
        return Reinst::BOL;
    case L'$':
        return Reinst::EOL;
    case L'[':
        return bldcclass();
    default:
        return Reinst::RUNE;
    }
}

////////////////////////////////////////////////////////////////////////////////
Reinst::OP regex_compiler::bldcclass()
{
    Reinst::OP type;
    dynarray<ucs4> runes;
    ucs4 rune;
    int quoted;

    // we have already seen the '['
    type = Reinst::CCLASS;
    _yyclassp = (*_prog->rclass.add() = new Reclass);

    // look ahead for negation
    // SPECIAL CASE!!! negated classes don't match \n

    quoted = nextc(&rune);
    if(!quoted && rune == L'^'){
        type = Reinst::NCCLASS;
        quoted = nextc(&rune);
        *runes.add() = '\n';
        *runes.add() = '\n';
        //*ep++ = L'\n';
        //*ep++ = L'\n';
    }

    // parse class into a set of spans
    while(1)
    {
        if(rune == 0){
            throw exception() << "malformed '[]'";
        }
        if(!quoted && rune == L']')
            break;
        if(!quoted && rune == L'-'){
            if(runes.size() == 0) {
                throw exception() << "malformed '[]'";
            }
            quoted = nextc(&rune);
            if((!quoted && rune == L']') || rune == 0){
                throw exception() << "malformed '[]'";
            }
            *runes.last() = rune;
        } else {
            *runes.add() = rune;
            *runes.add() = rune;
        }
        quoted = nextc(&rune);
    }

    // sort on span start
    ucs4* rend = runes.ptre();
    for( ucs4* p = runes.ptr(); p < rend; p+=2) {
        for( ucs4* np=p; np<rend; np+=2)
            if(*np < *p) {
                ucs4 rune = np[0];
                np[0] = p[0];
                p[0] = rune;
                rune = np[1];
                np[1] = p[1];
                p[1] = rune;
            }
    }

    // merge spans
    ucs4* p = runes.ptr();
    if(runes.size())
    {
        ucs4* np = _yyclassp->spans.need(2);
        np[0] = *p++;
        np[1] = *p++;

        for(; p<rend; p+=2)
            if(p[0] <= np[1]) {
                if(p[1] > np[1])
                    np[1] = p[1];
            } else {
                ucs4* np = _yyclassp->spans.add(2);
                np[0] = p[0];
                np[1] = p[1];
            }
    }

    return type;
}

////////////////////////////////////////////////////////////////////////////////
regex_program* regex_compiler::compile(token s, bool literal, Reinst::OP dot_type)
{
    // go compile the sucker
    _string = s;
    _nopenbraces = 0;
    _lastwasand = 0;
    _cursubid = 0;

    local<regex_program> prg = new regex_program;
    _prog = prg.ptr();

    // Start with a low priority operator to prime parser
    pushator(Reinst::OP(Reinst::START-1));

    Reinst::OP tk;
    while((tk = lex(literal, dot_type)) != Reinst::END)
    {
        if((tk & 0300) == Reinst::OPERATOR)
            xoperator(tk);
        else
            operand(tk);
    }

    // Close with a low priority operator
    evaluntil(Reinst::START);

    // Force END
    operand(Reinst::END);
    evaluntil(Reinst::START);

#ifdef DEBUG
    dumpstack();
#endif

    if(_nopenbraces)
        throw exception() << "unmatched left paren";

    // points to first and only operand
    prg->startinst = _andstack[0].first;
    _andstack.reset();

    prg->optimize();

    return prg.eject();
}

COID_NAMESPACE_END
