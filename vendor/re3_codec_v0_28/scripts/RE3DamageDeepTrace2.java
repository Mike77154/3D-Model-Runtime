// @category RE3
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
public class RE3DamageDeepTrace2 extends GhidraScript {
 @Override public void run() throws Exception {
  String[] args=getScriptArgs(); String target=args.length>0?args[0]:"/tmp/RE3_damage_deep.txt";
  PrintWriter w=new PrintWriter(new BufferedWriter(new FileWriter(target)));
  boolean mer=currentProgram.getName().toLowerCase().contains("merce");
  int d=mer?0x50:0;
  int[] seeds={0x4322a0,0x4368b0,0x436ed0,0x436f00,0x436e30,0x4367f0,0x431f70,0x436240,0x436270,0x4363b0,0x4364c0,0x436570,0x4365c0,0x436600,0x438780,0x438900,0x438985,0x436b20,0x436d90,0x436dd0,0x438200,0x427b80,0x429e20};
  w.println("PROGRAM="+currentProgram.getName());
  DecompInterface dec=new DecompInterface();dec.openProgram(currentProgram);
  for(int s:seeds){int adj=s+d;
   try {Address a=toAddr(String.format("%08x",adj));if(!currentProgram.getMemory().contains(a))continue;
     disassemble(a);Function f=currentProgram.getFunctionManager().getFunctionContaining(a);
     if(f==null)f=createFunction(a,"damage_deep_"+Integer.toHexString(adj));
     if(f==null)continue;
     DecompileResults r=dec.decompileFunction(f,20,monitor);
     w.println("\n===== " +Integer.toHexString(adj) + " FUNCTION "+f.getEntryPoint()+" LENGTH="+f.getBody().getNumAddresses()+" OK="+r.decompileCompleted()+" =====");
     if(r.decompileCompleted()){String c=r.getDecompiledFunction().getC();w.println(c.length()>34000?c.substring(0,34000):c);}else w.println(r.getErrorMessage());
   } catch(Exception e){w.println("ERROR "+Integer.toHexString(adj)+" " + e.toString());}}
  Address[] globals={toAddr("00A5E78C"),toAddr("00A5E790"),toAddr("00A5E77C"),toAddr("00A5E780")};
  for(Address g:globals){w.println("\nGLOBAL REFERENCES "+g);ReferenceIterator refs=currentProgram.getReferenceManager().getReferencesTo(g);int n=0;
    while(refs.hasNext()&&n<200){Reference ref=refs.next();w.println("  "+ref.getFromAddress()+" "+ref.getReferenceType());n++;}
    w.println("count="+n);
  }
  dec.dispose();w.close();println("RE3 damage deep trace saved "+target);
 }
}
