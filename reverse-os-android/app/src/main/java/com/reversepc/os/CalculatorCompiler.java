package com.reversepc.os;

import java.math.BigDecimal;

/** UI expression compiler. Arithmetic itself runs in the original XI machine.
 * Six decimal places, checked signed 64-bit operations, at most 16 registers. */
public final class CalculatorCompiler {
    private String input;private int position,stack,depth,serial;private StringBuilder code;
    private void whitespace(){while(position<input.length()&&Character.isWhitespace(input.charAt(position)))position++;}
    private boolean take(char c){whitespace();if(position<input.length()&&input.charAt(position)==c){position++;return true;}return false;}
    private void expression(){term();for(;;){if(take('+')){term();binary("add");}else if(take('-')){term();binary("sub");}else return;}}
    private void term(){factor();for(;;){if(take('*')){factor();binary("mul");}else if(take('/')){factor();binary("div");}else return;}}
    private void factor(){
        if(++depth>16)throw new IllegalArgumentException("Demasiados paréntesis");
        if(take('+'))factor();
        else if(take('-')){factor();code.append("mul r").append(stack-1).append(" -1\n");}
        else if(take('(')){expression();if(!take(')'))throw new IllegalArgumentException("Falta cerrar un paréntesis");}
        else{
            whitespace();int start=position;
            while(position<input.length()&&(Character.isDigit(input.charAt(position))||input.charAt(position)=='.'))position++;
            if(start==position||stack>=13)throw new IllegalArgumentException("Expresión no válida o demasiado compleja");
            try{long value=new BigDecimal(input.substring(start,position)).movePointRight(6).longValueExact();
                code.append("set r").append(stack++).append(' ').append(value).append('\n');
            }catch(ArithmeticException|NumberFormatException e){throw new IllegalArgumentException("Usa hasta seis decimales y una cantidad menor");}
        }
        depth--;
    }
    private void binary(String op){
        int left=stack-2,right=stack-1;
        if(op.equals("mul")||op.equals("div")){
            // Cancel common fixed-point factors in XI before multiplication,
            // so ordinary large integer operations do not overflow needlessly.
            String p="calc"+(serial++);
            code.append("mov r13 r").append(right).append("\ncmp r13 0\njge ").append(p).append("abs\nmul r13 -1\nlabel ").append(p).append("abs\nset r14 1000000\nlabel ").append(p).append("gcd\ncmp r14 0\nje ").append(p).append("ready\nmov r15 r13\nmod r15 r14\nmov r13 r14\nmov r14 r15\njmp ").append(p).append("gcd\nlabel ").append(p).append("ready\n");
            code.append("div r").append(right).append(" r13\nset r14 1000000\ndiv r14 r13\n");
            if(op.equals("mul"))code.append("mul r").append(left).append(" r").append(right).append("\ndiv r").append(left).append(" r14\n");
            else code.append("mul r").append(left).append(" r14\ndiv r").append(left).append(" r").append(right).append('\n');
        }else code.append(op).append(" r").append(left).append(" r").append(right).append('\n');
        stack--;
    }
    public String compile(String value){
        if(value==null||value.length()>256)throw new IllegalArgumentException("Expresión demasiado larga");
        input=value.replace('×','*').replace('÷','/').replace('−','-').replace(',','.');position=stack=depth=serial=0;
        code=new StringBuilder("#!reverse-pc-machine-v1\n");expression();whitespace();
        if(position!=input.length()||stack!=1)throw new IllegalArgumentException("Revisa la expresión");
        return code.append("print r0\nhalt\n").toString();
    }
    public static String format(long value){return BigDecimal.valueOf(value,6).stripTrailingZeros().toPlainString();}
}
